#include "launcher.h"
#include "config.h"
#include "settings.h"
#include "ui.h"

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <fileioc.h>
#include <graphx.h>
#include <ti/getcsc.h>
#include <ti/screen.h>
#include <ti/vars.h>

#define SELF_NAME     "CESECURE"
#define MAX_PROGRAMS  256
#define NAME_SIZE     9  /* 8 characters + NUL */
#define DESC_MAX      52

#define LIST_X        4
#define LIST_Y        (TOP_BAR_H + 6)
#define LIST_W        190
#define ROW_H         19
#define VISIBLE_ROWS  10
#define SCROLL_X      (LIST_X + LIST_W + 2)
#define PANEL_X       202
#define PANEL_W       (SCREEN_W - PANEL_X - 4)
#define PANEL_H       (VISIBLE_ROWS * ROW_H)

_Static_assert(LIST_Y + VISIBLE_ROWS * ROW_H <= BOTTOM_BAR_Y - 2, "list overlaps the bottom bar");

/* Ordered the way "Sort by type" lists them. */
enum { KIND_C, KIND_ICE, KIND_ASM, KIND_BASIC };
static const char *const kind_names[] = { "C", "ICE", "ASM", "BASIC" };
static const char *const kind_blurbs[] = { "C program", "ICE program", "Assembly program", "TI-BASIC program" };
static const uint8_t kind_colors[] = { COL_TAG_C, COL_TAG_ICE, COL_WARN, COL_TAG_BASIC };

#define FLAG_ARCHIVED  0x01
#define FLAG_HIDDEN    0x02
#define FLAG_FAV       0x04
#define FLAG_LOCKED    0x08

typedef struct {
    char name[NAME_SIZE];     /* VAT name, used to open and run */
    char display[NAME_SIZE];  /* shown name (hidden programs unmangled) */
    uint8_t type;
    uint8_t kind;
    uint8_t flags;
    uint16_t size;
    uint16_t icon_off;        /* offset of 16x16 icon pixels, 0 if none */
    uint16_t desc_off;        /* offset of description string, 0 if none */
} program_t;

static program_t programs[MAX_PROGRAMS];
static uint16_t program_count;

/* Icons for the visible rows. Consecutive indices map to distinct slots,
 * so a window of VISIBLE_ROWS programs never evicts itself. */
static uint8_t icon_data[VISIBLE_ROWS][2 + 16 * 16];
static uint16_t icon_owner[VISIBLE_ROWS];  /* program index + 1, 0 = empty */

static char desc[DESC_MAX + 1];
static uint16_t desc_owner;                 /* program index + 1, 0 = empty */

static void invalidate_caches(void)
{
    memset(icon_owner, 0, sizeof icon_owner);
    desc_owner = 0;
}

/* Hidden programs have their first letter lowered by 64. */
static bool is_hidden_name(const char *name)
{
    return name[0] >= 'A' - 64 && name[0] <= '[' - 64;
}

/* Reads the kind, size and where the icon/description live. Compiled
 * programs start with EF 7B (plus 00 for C, 7F for ICE); shells then expect
 * a jp instruction and a marker: 1 = 16x16 icon then description,
 * 2 = description only. This is the layout Cesium and CEaShell read. */
static void read_metadata(program_t *prgm)
{
    uint8_t header[12];
    uint8_t n;
    uint8_t base = 2;
    uint8_t marker;
    uint8_t handle = ti_OpenVar(prgm->name, "r", prgm->type);

    prgm->kind = KIND_BASIC;
    prgm->size = 0;
    prgm->icon_off = 0;
    prgm->desc_off = 0;
    if (!handle)
    {
        return;
    }
    prgm->size = ti_GetSize(handle);
    if (ti_IsArchived(handle))
    {
        prgm->flags |= FLAG_ARCHIVED;
    }
    n = prgm->size < sizeof header ? (uint8_t)prgm->size : (uint8_t)sizeof header;
    if (n && ti_Read(header, 1, n, handle) != n)
    {
        n = 0;
    }
    ti_Close(handle);

    if (n < 2 || header[0] != 0xEF || header[1] != 0x7B)
    {
        return;
    }
    prgm->kind = KIND_ASM;
    prgm->flags &= (uint8_t)~FLAG_LOCKED;
    if (n > 2 && header[2] == 0x00)
    {
        prgm->kind = KIND_C;
        base = 3;
    }
    else if (n > 2 && header[2] == 0x7F)
    {
        prgm->kind = KIND_ICE;
        base = 3;
    }
    if (n < base + 7 || header[base] != 0xC3)
    {
        return;
    }

    marker = base + 4;
    if (header[marker] == 1 && header[marker + 1] == 16 && header[marker + 2] == 16)
    {
        if (prgm->size >= marker + 3 + 16 * 16)
        {
            prgm->icon_off = marker + 3;
            prgm->desc_off = marker + 3 + 16 * 16;
        }
    }
    else if (header[marker] == 2)
    {
        prgm->desc_off = marker + 1;
    }
    if (prgm->desc_off >= prgm->size)
    {
        prgm->desc_off = 0;
    }
}

static int compare_programs(const void *a, const void *b)
{
    const program_t *x = a;
    const program_t *y = b;
    int fav_x = (x->flags & FLAG_FAV) != 0;
    int fav_y = (y->flags & FLAG_FAV) != 0;

    if (fav_x != fav_y)
    {
        return fav_y - fav_x;
    }
    if (config.sort == SORT_TYPE && x->kind != y->kind)
    {
        return x->kind - y->kind;
    }
    if (config.sort == SORT_SIZE && x->size != y->size)
    {
        return x->size < y->size ? 1 : -1;
    }
    return strcmp(x->display, y->display);
}

static void sort_programs(void)
{
    qsort(programs, program_count, sizeof programs[0], compare_programs);
    invalidate_caches();
}

static void load_programs(void)
{
    void *pos = NULL;
    const char *name;
    uint8_t type;

    program_count = 0;
    while ((name = ti_DetectAny(&pos, NULL, &type)) != NULL)
    {
        program_t *prgm;
        size_t len;
        bool hidden;

        if (type != OS_TYPE_PRGM && type != OS_TYPE_PROT_PRGM)
        {
            continue;
        }
        /* Names below 'A' are system programs, except hidden ones. */
        hidden = is_hidden_name(name);
        if ((name[0] < 'A' && !hidden) || (hidden && !config.show_hidden))
        {
            continue;
        }
        if (strcmp(name, SELF_NAME) == 0)
        {
            continue;
        }
        len = strlen(name);
        if (len >= NAME_SIZE)
        {
            continue;
        }
        if (program_count >= MAX_PROGRAMS)
        {
            break;
        }

        prgm = &programs[program_count++];
        memcpy(prgm->name, name, len + 1);
        memcpy(prgm->display, name, len + 1);
        prgm->type = type;
        /* Compiled programs are always stored protected, so "locked" only
         * means something for TI-BASIC; read_metadata() clears it for them. */
        prgm->flags = type == OS_TYPE_PROT_PRGM ? FLAG_LOCKED : 0;
        if (hidden)
        {
            prgm->display[0] += 64;
            prgm->flags |= FLAG_HIDDEN;
        }
    }

    /* Open the variables only after the VAT walk is finished. */
    for (uint16_t i = 0; i < program_count; i++)
    {
        read_metadata(&programs[i]);
        if (config_is_favorite(programs[i].name))
        {
            programs[i].flags |= FLAG_FAV;
        }
    }

    sort_programs();
}

static uint16_t index_of(const char *name)
{
    for (uint16_t i = 0; i < program_count; i++)
    {
        if (strcmp(programs[i].name, name) == 0)
        {
            return i;
        }
    }
    return 0;
}

static const gfx_sprite_t *get_icon(uint16_t index)
{
    const program_t *prgm = &programs[index];
    uint8_t slot = index % VISIBLE_ROWS;
    uint8_t *buf = icon_data[slot];

    if (!prgm->icon_off)
    {
        return NULL;
    }
    if (icon_owner[slot] != index + 1)
    {
        uint8_t handle = ti_OpenVar(prgm->name, "r", prgm->type);
        bool ok = handle &&
                  ti_Seek(prgm->icon_off, SEEK_SET, handle) != EOF &&
                  ti_Read(buf + 2, 16 * 16, 1, handle) == 1;

        if (handle)
        {
            ti_Close(handle);
        }
        if (!ok)
        {
            programs[index].icon_off = 0;
            icon_owner[slot] = 0;
            return NULL;
        }
        buf[0] = 16;
        buf[1] = 16;
        for (unsigned int i = 2; i < sizeof icon_data[0]; i++)
        {
            buf[i] = ui_icon_pixel(buf[i]);
        }
        icon_owner[slot] = index + 1;
    }
    return (const gfx_sprite_t *)buf;
}

static const char *get_description(uint16_t index)
{
    const program_t *prgm = &programs[index];

    if (desc_owner != index + 1)
    {
        desc_owner = index + 1;
        desc[0] = '\0';
        if (prgm->desc_off)
        {
            uint8_t handle = ti_OpenVar(prgm->name, "r", prgm->type);
            uint16_t avail = prgm->size - prgm->desc_off;
            uint8_t want = avail < DESC_MAX ? (uint8_t)avail : DESC_MAX;

            if (handle)
            {
                if (ti_Seek(prgm->desc_off, SEEK_SET, handle) == EOF ||
                    ti_Read(desc, 1, want, handle) != want)
                {
                    want = 0;
                }
                ti_Close(handle);
                desc[want] = '\0';
            }
            /* Stop at the terminator or anything that isn't printable. */
            for (char *c = desc; *c; c++)
            {
                if (*c < ' ' || *c > '~')
                {
                    *c = '\0';
                    break;
                }
            }
        }
    }
    return desc[0] ? desc : NULL;
}

/* Colored square with the type's first letter, for programs with no icon. */
static void draw_badge(const program_t *prgm, int x, int y, uint8_t scale)
{
    char letter[2] = { kind_names[prgm->kind][0], '\0' };
    int size = 16 * scale;

    ui_round_rect(x, y, size, size, kind_colors[prgm->kind]);
    gfx_SetTextScale(scale, scale);
    ui_text(letter, x + (size - 7 * scale) / 2 + 1, y + (size - 8 * scale) / 2 + 1, COL_BG);
    gfx_SetTextScale(1, 1);
}

static void draw_row(uint16_t index, int y, bool selected)
{
    const program_t *prgm = &programs[index];
    const gfx_sprite_t *icon = get_icon(index);
    const char *tag = kind_names[prgm->kind];
    int tag_x = LIST_X + LIST_W - 6 - (int)gfx_GetStringWidth(tag);
    uint8_t name_color = (prgm->flags & FLAG_HIDDEN) ? COL_DIM : COL_TEXT;

    if (selected)
    {
        ui_round_rect(LIST_X, y, LIST_W, ROW_H, COL_ACCENT);
        name_color = COL_ON_ACCENT;
    }

    if (icon)
    {
        gfx_Sprite_NoClip(icon, LIST_X + 3, y + 1);
    }
    else
    {
        draw_badge(prgm, LIST_X + 3, y + 1, 1);
    }

    ui_text(prgm->display, LIST_X + 26, y + 6, name_color);
    ui_text(tag, tag_x, y + 6, selected ? COL_ON_ACCENT : kind_colors[prgm->kind]);
    if (prgm->flags & FLAG_FAV)
    {
        ui_draw_star(tag_x - 12, y + 6, selected ? COL_ON_ACCENT : COL_FAV);
    }
}

static void draw_info_row(const char *label, const char *value, int y)
{
    ui_text(label, PANEL_X + 8, y, COL_DIM);
    ui_text(value, PANEL_X + PANEL_W - 8 - (int)gfx_GetStringWidth(value), y, COL_TEXT);
}

static void draw_panel(uint16_t index)
{
    const program_t *prgm = &programs[index];
    const gfx_sprite_t *icon = get_icon(index);
    const char *description = get_description(index);
    const char *tag = kind_names[prgm->kind];
    int cx = PANEL_X + PANEL_W / 2;
    int tag_w = (int)gfx_GetStringWidth(tag) + 10;
    char buf[16];

    ui_round_rect(PANEL_X, LIST_Y, PANEL_W, PANEL_H, COL_PANEL);

    /* Large icon */
    if (icon)
    {
        gfx_SetColor(COL_LINE);
        gfx_Rectangle_NoClip(cx - 17, LIST_Y + 7, 34, 34);
        gfx_ScaledSprite_NoClip(icon, cx - 16, LIST_Y + 8, 2, 2);
    }
    else
    {
        draw_badge(prgm, cx - 16, LIST_Y + 8, 2);
    }
    if (prgm->flags & FLAG_FAV)
    {
        ui_draw_star(PANEL_X + PANEL_W - 13, LIST_Y + 6, COL_FAV);
    }

    ui_text(prgm->display, cx - (int)gfx_GetStringWidth(prgm->display) / 2, LIST_Y + 46, COL_TEXT);
    ui_round_rect(cx - tag_w / 2, LIST_Y + 58, tag_w, 12, kind_colors[prgm->kind]);
    ui_text(tag, cx - tag_w / 2 + 5, LIST_Y + 60, COL_BG);

    ui_append(ui_append_uint(buf, prgm->size), " B");
    draw_info_row("Size", buf, LIST_Y + 78);
    draw_info_row("In", (prgm->flags & FLAG_ARCHIVED) ? "Archive" : "RAM", LIST_Y + 90);
    if (prgm->flags & (FLAG_LOCKED | FLAG_HIDDEN))
    {
        char *p = buf;

        buf[0] = '\0';
        if (prgm->flags & FLAG_LOCKED)
        {
            p = ui_append(p, "Locked");
        }
        if (prgm->flags & FLAG_HIDDEN)
        {
            ui_append(p, p != buf ? " Hidden" : "Hidden");
        }
        ui_text(buf, PANEL_X + 8, LIST_Y + 102, COL_WARN);
    }

    gfx_SetColor(COL_LINE);
    gfx_HorizLine_NoClip(PANEL_X + 8, LIST_Y + 116, PANEL_W - 16);
    ui_text_wrapped(description ? description : kind_blurbs[prgm->kind],
                    PANEL_X + 8, LIST_Y + 122, PANEL_W - 16, 6,
                    description ? COL_TEXT : COL_DIM);
}

static void draw_scrollbar(uint16_t top)
{
    const int track_h = VISIBLE_ROWS * ROW_H;
    int thumb_h = track_h * VISIBLE_ROWS / program_count;
    int thumb_y;

    if (thumb_h < 8)
    {
        thumb_h = 8;
    }
    thumb_y = LIST_Y + (track_h - thumb_h) * top / (program_count - VISIBLE_ROWS);

    gfx_SetColor(COL_LINE);
    gfx_FillRectangle_NoClip(SCROLL_X, LIST_Y, 3, track_h);
    gfx_SetColor(COL_ACCENT);
    gfx_FillRectangle_NoClip(SCROLL_X, thumb_y, 3, thumb_h);
}

static void draw_list(uint16_t sel, uint16_t top)
{
    static const char *const chips[] = {
        "enter", "Run", "y=", "Fav", "mode", "Menu", "clear", "Quit", NULL
    };
    static const char *const empty_chips[] = { "mode", "Menu", "clear", "Quit", NULL };
    char context[16];

    if (!program_count)
    {
        ui_frame("Programs", empty_chips);
        ui_round_rect(40, 70, SCREEN_W - 80, 90, COL_PANEL);
        ui_text_centered("No programs yet", 96, COL_TEXT, 1);
        ui_text_centered("Send some with TI Connect CE", 114, COL_DIM, 1);
        ui_text_centered("and they will show up here.", 126, COL_DIM, 1);
        gfx_SwapDraw();
        return;
    }

    ui_append_uint(ui_append(ui_append_uint(context, sel + 1u), "/"), program_count);
    ui_frame(context, chips);

    for (uint8_t row = 0; row < VISIBLE_ROWS && top + row < program_count; row++)
    {
        draw_row(top + row, LIST_Y + row * ROW_H, top + row == sel);
    }
    if (program_count > VISIBLE_ROWS)
    {
        draw_scrollbar(top);
    }
    draw_panel(sel);

    gfx_SwapDraw();
}

/* Keeps the selected row inside the visible window. */
static uint16_t scroll_to(uint16_t sel, uint16_t top)
{
    if (sel < top)
    {
        return sel;
    }
    if (sel >= top + VISIBLE_ROWS)
    {
        return sel - VISIBLE_ROWS + 1;
    }
    return top;
}

/* The green letter above each key, as on the TI-84 Plus CE keypad. Returns
 * 0 for keys without a letter. Theta is '[' in the TI character set. */
static char key_to_letter(uint8_t key)
{
    static const uint8_t letter_keys[27] = {
        sk_Math, sk_Apps, sk_Prgm, sk_Recip, sk_Sin, sk_Cos, sk_Tan, sk_Power,
        sk_Square, sk_Comma, sk_LParen, sk_RParen, sk_Div, sk_Log, sk_7, sk_8,
        sk_9, sk_Mul, sk_Ln, sk_4, sk_5, sk_6, sk_Sub, sk_Store, sk_1, sk_2, sk_3,
    };

    for (uint8_t i = 0; i < sizeof letter_keys; i++)
    {
        if (letter_keys[i] == key)
        {
            return (char)('A' + i);
        }
    }
    return 0;
}

/* Moves to the next program (after sel, wrapping) starting with letter. */
static uint16_t jump_to_letter(uint16_t sel, char letter)
{
    for (uint16_t step = 1; step <= program_count; step++)
    {
        uint16_t i = (sel + step) % program_count;

        if (programs[i].display[0] == letter)
        {
            return i;
        }
    }
    return sel;
}

/* Entry point after a launched program exits. Only reachable from an
 * unlocked session, so the PIN is not asked again. */
static int after_program(void *data, int retval)
{
    (void)retval;

    config_load();
    ui_init();
    launcher_run((const char *)data);
    config_save();
    ui_quit();

    return 0;
}

static void run_program(const program_t *prgm)
{
    char line[40];
    int ret;

    /* Everything in RAM is lost once the program starts. */
    config_save();
    ui_end();
    os_ClrHomeFull();

    /* The name is copied as callback data so the cursor can return to it. */
    ret = os_RunPrgm(prgm->name, (void *)prgm->name, strlen(prgm->name) + 1, after_program);

    /* Only reached if the program could not be started. */
    ui_init();
    ui_append(ui_append(line, "Could not run "), prgm->display);
    ui_message("Error", "Launch failed", COL_ERR, line,
               ret == OS_RUN_PRGM_ERR_MEMORY ? "Not enough free RAM." : "The program was not found.");
}

void launcher_run(const char *reselect)
{
    char keep[NAME_SIZE];
    uint16_t sel = 0;
    uint16_t top = 0;

    load_programs();
    if (reselect)
    {
        sel = index_of(reselect);
    }
    top = scroll_to(sel, top);

    for (;;)
    {
        uint8_t key;
        char letter;

        draw_list(sel, top);
        key = ui_poll_key();

        switch (key)
        {
            case 0:  /* clock ticked; redraw */
                break;
            case sk_Up:
                if (program_count)
                {
                    sel = sel > 0 ? sel - 1 : program_count - 1;
                }
                break;
            case sk_Down:
                if (program_count)
                {
                    sel = sel + 1 < program_count ? sel + 1 : 0;
                }
                break;
            case sk_Left:
                sel = sel > VISIBLE_ROWS ? sel - VISIBLE_ROWS : 0;
                break;
            case sk_Right:
                if (program_count)
                {
                    sel = sel + VISIBLE_ROWS < program_count ? sel + VISIBLE_ROWS : program_count - 1;
                }
                break;
            case sk_Enter:
            case sk_2nd:
                if (program_count)
                {
                    run_program(&programs[sel]);
                }
                break;
            case sk_Yequ:
                if (program_count)
                {
                    strcpy(keep, programs[sel].name);
                    if (config_toggle_favorite(keep))
                    {
                        programs[sel].flags ^= FLAG_FAV;
                        sort_programs();
                        sel = index_of(keep);
                    }
                    else
                    {
                        ui_message("Favorites", "List full", COL_WARN,
                                   "You can pin up to 16 favorites.", "Remove one with [y=] first.");
                    }
                }
                break;
            case sk_Mode:
                keep[0] = '\0';
                if (program_count)
                {
                    strcpy(keep, programs[sel].name);
                }
                settings_run();
                load_programs();
                sel = index_of(keep);
                break;
            case sk_Clear:
                return;
            default:
                letter = key_to_letter(key);
                if (letter && program_count)
                {
                    sel = jump_to_letter(sel, letter);
                }
                break;
        }

        top = scroll_to(sel, top);
    }
}

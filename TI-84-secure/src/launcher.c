#include "launcher.h"
#include "pin.h"
#include "ui.h"

#include <stdbool.h>
#include <stdint.h>
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

#define LIST_X        4
#define LIST_Y        (TOP_BAR_H + 4)
#define LIST_W        (SCREEN_W - LIST_X - 12)
#define ROW_H         14
#define VISIBLE_ROWS  13
#define SCROLL_X      (SCREEN_W - 8)

_Static_assert(LIST_Y + VISIBLE_ROWS * ROW_H <= BOTTOM_BAR_Y, "list overlaps the bottom bar");

typedef struct {
    char name[NAME_SIZE];
    uint8_t type;
    bool is_asm;
} program_t;

static program_t programs[MAX_PROGRAMS];
static uint16_t program_count;

static int compare_programs(const void *a, const void *b)
{
    return strcmp(((const program_t *)a)->name, ((const program_t *)b)->name);
}

/* Compiled programs start with the tExtTok, tAsm84CeCmp token pair. */
static bool detect_asm(const program_t *prgm)
{
    uint8_t header[2];
    uint8_t handle = ti_OpenVar(prgm->name, "r", prgm->type);
    bool is_asm = false;

    if (handle)
    {
        is_asm = ti_Read(header, sizeof header, 1, handle) == 1 &&
                 header[0] == 0xEF && header[1] == 0x7B;
        ti_Close(handle);
    }
    return is_asm;
}

static void load_programs(void)
{
    void *pos = NULL;
    const char *name;
    uint8_t type;

    program_count = 0;
    while ((name = ti_DetectAny(&pos, NULL, &type)) != NULL)
    {
        size_t len;

        if (type != OS_TYPE_PRGM && type != OS_TYPE_PROT_PRGM)
        {
            continue;
        }
        /* Hidden and system programs start below 'A'. */
        if (name[0] < 'A' || strcmp(name, SELF_NAME) == 0)
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
        memcpy(programs[program_count].name, name, len + 1);
        programs[program_count].type = type;
        program_count++;
    }

    /* Open the variables only after the VAT walk is finished. */
    for (uint16_t i = 0; i < program_count; i++)
    {
        programs[i].is_asm = detect_asm(&programs[i]);
    }

    qsort(programs, program_count, sizeof programs[0], compare_programs);
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

static void draw_list(uint16_t sel, uint16_t top)
{
    char context[16];

    if (program_count)
    {
        ui_append_uint(ui_append(ui_append_uint(context, sel + 1u), "/"), program_count);
    }
    else
    {
        strcpy(context, "Launcher");
    }
    ui_frame(context, "enter:run  mode:PIN  clear:quit");

    if (!program_count)
    {
        ui_text_centered("No programs found", 104, COL_TEXT, 1);
        ui_text_centered("Send some with TI Connect CE", 120, COL_DIM, 1);
        gfx_SwapDraw();
        return;
    }

    for (uint8_t row = 0; row < VISIBLE_ROWS; row++)
    {
        uint16_t index = top + row;
        const program_t *prgm;
        const char *tag;
        int y = LIST_Y + row * ROW_H;
        bool selected = index == sel;

        if (index >= program_count)
        {
            break;
        }
        prgm = &programs[index];
        tag = prgm->is_asm ? "ASM" : "BASIC";

        if (selected)
        {
            gfx_SetColor(COL_ACCENT);
            gfx_FillRectangle_NoClip(LIST_X, y, LIST_W, ROW_H);
        }
        ui_text(prgm->name, LIST_X + 8, y + 3, COL_TEXT);
        ui_text(tag, LIST_X + LIST_W - 8 - (int)gfx_GetStringWidth(tag), y + 3,
                selected ? COL_TEXT : (prgm->is_asm ? COL_ASM : COL_BASIC));
    }

    if (program_count > VISIBLE_ROWS)
    {
        const int track_h = VISIBLE_ROWS * ROW_H;
        int thumb_h = track_h * VISIBLE_ROWS / program_count;
        int thumb_y;

        if (thumb_h < 8)
        {
            thumb_h = 8;
        }
        thumb_y = LIST_Y + (track_h - thumb_h) * top / (program_count - VISIBLE_ROWS);

        gfx_SetColor(COL_BAR);
        gfx_FillRectangle_NoClip(SCROLL_X, LIST_Y, 4, track_h);
        gfx_SetColor(COL_DIM);
        gfx_FillRectangle_NoClip(SCROLL_X, thumb_y, 4, thumb_h);
    }

    gfx_SwapDraw();
}

/* Entry point after a launched program exits. Only reachable from an
 * unlocked session, so the PIN is not asked again. */
static int after_program(void *data, int retval)
{
    (void)retval;

    ui_init();
    launcher_run((const char *)data);
    ui_quit();

    return 0;
}

static void run_program(const program_t *prgm)
{
    char line[40];
    int ret;

    ui_end();
    os_ClrHomeFull();

    /* The name is copied as callback data so the cursor can return to it. */
    ret = os_RunPrgm(prgm->name, (void *)prgm->name, strlen(prgm->name) + 1, after_program);

    /* Only reached if the program could not be started. */
    ui_init();
    ui_append(ui_append(line, "Could not run "), prgm->name);
    ui_message("Error", "Launch failed", COL_ERR, line,
               ret == OS_RUN_PRGM_ERR_MEMORY ? "Not enough free RAM." : "The program was not found.");
}

void launcher_run(const char *reselect)
{
    uint16_t sel = 0;
    uint16_t top = 0;

    load_programs();

    if (reselect)
    {
        for (uint16_t i = 0; i < program_count; i++)
        {
            if (strcmp(programs[i].name, reselect) == 0)
            {
                sel = i;
                break;
            }
        }
    }
    top = scroll_to(sel, top);

    for (;;)
    {
        draw_list(sel, top);

        switch (ui_wait_key())
        {
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
            case sk_Enter:
                if (program_count)
                {
                    run_program(&programs[sel]);
                }
                break;
            case sk_Mode:
                pin_change();
                break;
            case sk_Clear:
                return;
            default:
                break;
        }

        top = scroll_to(sel, top);
    }
}

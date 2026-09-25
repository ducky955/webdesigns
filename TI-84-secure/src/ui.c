#include "ui.h"
#include "config.h"

#include <stdbool.h>
#include <string.h>

#include <graphx.h>
#include <sys/power.h>
#include <sys/rtc.h>
#include <ti/getcsc.h>
#include <ti/screen.h>

#define RGB(r, g, b) gfx_RGBTo1555(r, g, b)

typedef struct {
    const char *name;
    /* In enum order: BG, PANEL, BAR, TEXT, DIM, ACCENT, ON_ACCENT, ERR, OK,
     * WARN, TAG_C, TAG_BASIC, TAG_ICE, FAV, LINE */
    uint16_t colors[THEME_SLOTS];
} theme_t;

static const theme_t themes[] = {
    { "Midnight", {
        RGB(16, 18, 26),   RGB(28, 32, 46),   RGB(34, 38, 54),   RGB(232, 234, 242),
        RGB(128, 136, 160), RGB(58, 128, 246), RGB(255, 255, 255), RGB(248, 92, 92),
        RGB(92, 208, 132), RGB(250, 184, 72), RGB(110, 196, 240), RGB(176, 150, 252),
        RGB(250, 128, 200), RGB(255, 208, 72), RGB(52, 58, 80) } },
    { "Ocean", {
        RGB(8, 24, 32),    RGB(14, 40, 52),   RGB(18, 50, 64),   RGB(224, 244, 248),
        RGB(112, 160, 172), RGB(0, 176, 196),  RGB(4, 22, 28),    RGB(248, 104, 96),
        RGB(96, 216, 160), RGB(248, 196, 88), RGB(120, 216, 248), RGB(168, 176, 248),
        RGB(248, 144, 200), RGB(255, 216, 96), RGB(30, 70, 86) } },
    { "Forest", {
        RGB(14, 22, 16),   RGB(24, 36, 26),   RGB(30, 46, 32),   RGB(230, 240, 226),
        RGB(128, 156, 128), RGB(104, 196, 112), RGB(8, 20, 10),   RGB(240, 100, 88),
        RGB(160, 224, 120), RGB(232, 196, 88), RGB(128, 208, 216), RGB(200, 176, 240),
        RGB(240, 150, 190), RGB(248, 216, 96), RGB(44, 66, 48) } },
    { "Ember", {
        RGB(24, 14, 14),   RGB(40, 24, 22),   RGB(52, 28, 26),   RGB(246, 232, 226),
        RGB(172, 132, 124), RGB(240, 100, 64), RGB(255, 255, 255), RGB(255, 84, 84),
        RGB(120, 216, 140), RGB(252, 192, 80), RGB(128, 196, 240), RGB(200, 160, 248),
        RGB(252, 140, 180), RGB(255, 208, 80), RGB(72, 42, 38) } },
    { "Synthwave", {
        RGB(20, 12, 36),   RGB(34, 20, 58),   RGB(44, 24, 74),   RGB(242, 232, 255),
        RGB(152, 132, 196), RGB(255, 64, 164), RGB(255, 255, 255), RGB(255, 96, 96),
        RGB(64, 232, 200), RGB(255, 200, 72), RGB(64, 208, 255), RGB(180, 140, 255),
        RGB(255, 120, 220), RGB(255, 224, 64), RGB(66, 40, 104) } },
    { "Graphite", {
        RGB(18, 18, 18),   RGB(30, 30, 30),   RGB(38, 38, 38),   RGB(236, 236, 236),
        RGB(140, 140, 140), RGB(210, 210, 210), RGB(16, 16, 16),  RGB(240, 96, 96),
        RGB(120, 208, 140), RGB(232, 192, 96), RGB(150, 190, 220), RGB(190, 170, 220),
        RGB(220, 160, 190), RGB(240, 210, 110), RGB(58, 58, 58) } },
    { "Seashell", {
        RGB(250, 238, 222), RGB(242, 218, 190), RGB(238, 164, 112), RGB(46, 30, 20),
        RGB(132, 100, 78), RGB(222, 124, 68), RGB(255, 255, 255), RGB(196, 40, 40),
        RGB(40, 140, 72),  RGB(184, 104, 0),  RGB(24, 110, 168), RGB(118, 72, 176),
        RGB(184, 40, 120), RGB(200, 136, 0),  RGB(224, 196, 164) } },
};

#define THEME_COUNT (sizeof themes / sizeof themes[0])

/* Icon pixels that fall in the themed slots are drawn with the closest
 * untouched default color instead. Rebuilt on every ui_init(). */
static uint8_t icon_remap[THEME_SLOTS];

static uint16_t color_distance(uint16_t a, uint16_t b)
{
    int dr = (int)((a >> 10) & 31) - (int)((b >> 10) & 31);
    int dg = (int)((a >> 5) & 31) - (int)((b >> 5) & 31);
    int db = (int)(a & 31) - (int)(b & 31);

    return (uint16_t)(dr * dr + dg * dg + db * db);
}

static void build_icon_remap(void)
{
    for (uint8_t slot = 0; slot < THEME_SLOTS; slot++)
    {
        uint16_t want = gfx_palette[COL_BG + slot];
        uint16_t best_distance = UINT16_MAX;
        uint8_t best = 0;

        for (unsigned int i = 0; i < 256; i++)
        {
            uint16_t d;

            if (i >= COL_BG && i <= COL_LINE)
            {
                continue;
            }
            d = color_distance(want, gfx_palette[i]);
            if (d < best_distance)
            {
                best_distance = d;
                best = (uint8_t)i;
            }
        }
        icon_remap[slot] = best;
    }
}

uint8_t ui_icon_pixel(uint8_t index)
{
    if (index >= COL_BG && index <= COL_LINE)
    {
        return icon_remap[index - COL_BG];
    }
    return index;
}

uint8_t ui_theme_count(void)
{
    return THEME_COUNT;
}

const char *ui_theme_name(uint8_t theme)
{
    return themes[theme < THEME_COUNT ? theme : 0].name;
}

void ui_apply_theme(uint8_t theme)
{
    const uint16_t *colors = themes[theme < THEME_COUNT ? theme : 0].colors;

    for (uint8_t i = 0; i < THEME_SLOTS; i++)
    {
        gfx_palette[COL_BG + i] = colors[i];
    }
}

void ui_init(void)
{
    gfx_Begin();

    /* gfx_Begin loaded the default palette; remember it before theming. */
    build_icon_remap();
    ui_apply_theme(config.theme);

    gfx_SetDrawBuffer();
    gfx_SetTextTransparentColor(COL_TRANSPARENT);
    gfx_SetTextBGColor(COL_TRANSPARENT);
    gfx_SetTextScale(1, 1);
}

void ui_end(void)
{
    gfx_End();
}

void ui_quit(void)
{
    gfx_End();
    os_ClrHomeFull();
}

void ui_text(const char *s, int x, int y, uint8_t color)
{
    gfx_SetTextFGColor(color);
    gfx_PrintStringXY(s, x, y);
}

void ui_text_centered(const char *s, int y, uint8_t color, uint8_t scale)
{
    /* Measure at scale 1 so the result doesn't depend on whether
     * gfx_GetStringWidth applies the current text scale. */
    int width;

    gfx_SetTextScale(1, 1);
    width = (int)gfx_GetStringWidth(s) * scale;
    gfx_SetTextScale(scale, scale);
    ui_text(s, (SCREEN_W - width) / 2, y, color);
    gfx_SetTextScale(1, 1);
}

uint8_t ui_text_wrapped(const char *s, int x, int y, int w, uint8_t max_lines, uint8_t color)
{
    char line[48];
    uint8_t lines = 0;

    while (*s && lines < max_lines)
    {
        uint8_t len = 0;
        uint8_t take;
        uint8_t last_space = 0;
        int width = 0;

        while (*s == ' ')
        {
            s++;
        }
        if (!*s)
        {
            break;
        }

        /* Fit as many characters as possible, then back up to a space. */
        while (s[len] && len < sizeof line - 1)
        {
            int cw = (int)gfx_GetCharWidth(s[len]);

            if (width + cw > w)
            {
                break;
            }
            if (s[len] == ' ')
            {
                last_space = len;
            }
            width += cw;
            len++;
        }
        take = len;
        if (s[len] && s[len] != ' ' && last_space)
        {
            take = last_space;
        }
        if (!take)
        {
            take = 1;
        }

        memcpy(line, s, take);
        line[take] = '\0';
        ui_text(line, x, y + lines * 10, color);
        lines++;
        s += take;
    }
    return lines;
}

void ui_round_rect(int x, int y, int w, int h, uint8_t color)
{
    gfx_SetColor(color);
    gfx_FillRectangle_NoClip(x + 2, y, w - 4, h);
    gfx_FillRectangle_NoClip(x + 1, y + 1, w - 2, h - 2);
    gfx_FillRectangle_NoClip(x, y + 2, w, h - 4);
}

static void draw_mini_lock(int x, int y)
{
    gfx_SetColor(COL_ACCENT);
    gfx_FillCircle_NoClip(x + 5, y + 4, 4);
    gfx_SetColor(COL_BAR);
    gfx_FillCircle_NoClip(x + 5, y + 4, 2);
    ui_round_rect(x, y + 5, 11, 8, COL_ACCENT);
}

static void draw_clock_and_battery(void)
{
    char clock[12];
    char *p = clock;
    uint8_t seconds, minutes, hours;
    uint8_t level = boot_GetBatteryStatus();
    uint8_t fill_color;
    int x;

    boot_GetTime(&seconds, &minutes, &hours);
    if (config.clock24)
    {
        if (hours < 10)
        {
            *p++ = '0';
        }
        p = ui_append_uint(p, hours);
    }
    else
    {
        p = ui_append_uint(p, hours % 12 ? hours % 12 : 12);
    }
    *p++ = ':';
    if (minutes < 10)
    {
        *p++ = '0';
    }
    p = ui_append_uint(p, minutes);
    if (!config.clock24)
    {
        ui_append(p, hours < 12 ? " AM" : " PM");
    }

    /* Battery: outline, nub, and up to four bars. */
    x = SCREEN_W - 26;
    gfx_SetColor(COL_TEXT);
    gfx_Rectangle_NoClip(x, 5, 18, 10);
    gfx_FillRectangle_NoClip(x + 18, 8, 2, 4);
    if (level > 4)
    {
        level = 4;
    }
    fill_color = boot_BatteryCharging() ? COL_ACCENT : (level <= 1 ? COL_ERR : COL_OK);
    gfx_SetColor(fill_color);
    for (uint8_t i = 0; i < level; i++)
    {
        gfx_FillRectangle_NoClip(x + 2 + i * 4, 7, 3, 6);
    }

    ui_text(clock, x - 8 - (int)gfx_GetStringWidth(clock), 6, COL_TEXT);
}

static void draw_chips(const char *const *chips)
{
    int total = 0;
    int x;

    for (const char *const *c = chips; c[0] && c[1]; c += 2)
    {
        total += (int)gfx_GetStringWidth(c[0]) + 8 + 4 + (int)gfx_GetStringWidth(c[1]) + 10;
    }
    x = (SCREEN_W - (total - 10)) / 2;

    for (const char *const *c = chips; c[0] && c[1]; c += 2)
    {
        int key_w = (int)gfx_GetStringWidth(c[0]) + 8;

        ui_round_rect(x, BOTTOM_BAR_Y + 3, key_w, 12, COL_LINE);
        ui_text(c[0], x + 4, BOTTOM_BAR_Y + 5, COL_TEXT);
        x += key_w + 4;
        ui_text(c[1], x, BOTTOM_BAR_Y + 5, COL_DIM);
        x += (int)gfx_GetStringWidth(c[1]) + 10;
    }
}

void ui_frame(const char *context, const char *const *chips)
{
    gfx_FillScreen(COL_BG);

    gfx_SetColor(COL_BAR);
    gfx_FillRectangle_NoClip(0, 0, SCREEN_W, TOP_BAR_H);
    gfx_FillRectangle_NoClip(0, BOTTOM_BAR_Y, SCREEN_W, BOTTOM_BAR_H);
    gfx_SetColor(COL_ACCENT);
    gfx_HorizLine_NoClip(0, TOP_BAR_H, SCREEN_W);

    draw_mini_lock(6, 3);
    ui_text("CESecure", 22, 6, COL_TEXT);
    if (context)
    {
        ui_text(context, 22 + (int)gfx_GetStringWidth("CESecure") + 10, 6, COL_DIM);
    }
    draw_clock_and_battery();

    if (chips)
    {
        draw_chips(chips);
    }
}

void ui_draw_lock(int cx, int y, uint8_t color)
{
    /* Shackle: a ring whose lower half is hidden behind the body. */
    gfx_SetColor(color);
    gfx_FillCircle_NoClip(cx, y + 14, 14);
    gfx_SetColor(COL_BG);
    gfx_FillCircle_NoClip(cx, y + 14, 9);

    /* Body */
    ui_round_rect(cx - 20, y + 20, 40, 30, color);

    /* Keyhole */
    gfx_SetColor(COL_BG);
    gfx_FillCircle_NoClip(cx, y + 31, 4);
    gfx_FillRectangle_NoClip(cx - 2, y + 31, 4, 10);
}

void ui_draw_star(int x, int y, uint8_t color)
{
    static const uint8_t rows[7] = { 0x08, 0x1C, 0x7F, 0x3E, 0x1C, 0x36, 0x63 };

    gfx_SetColor(color);
    for (uint8_t r = 0; r < 7; r++)
    {
        for (uint8_t c = 0; c < 7; c++)
        {
            if (rows[r] & (0x40 >> c))
            {
                gfx_SetPixel(x + c, y + r);
            }
        }
    }
}

char *ui_append(char *dst, const char *s)
{
    while ((*dst = *s++))
    {
        dst++;
    }
    return dst;
}

char *ui_append_uint(char *dst, unsigned int n)
{
    char digits[8];
    uint8_t count = 0;

    do
    {
        digits[count++] = (char)('0' + n % 10);
        n /= 10;
    } while (n);

    while (count)
    {
        *dst++ = digits[--count];
    }
    *dst = '\0';
    return dst;
}

uint8_t ui_wait_key(void)
{
    uint8_t key;

    while (!(key = os_GetCSC()));

    return key;
}

uint8_t ui_poll_key(void)
{
    uint8_t minute = rtc_Minutes;

    for (;;)
    {
        uint8_t key = os_GetCSC();

        if (key)
        {
            return key;
        }
        if (rtc_Minutes != minute)
        {
            return 0;
        }
    }
}

void ui_message(const char *context, const char *title, uint8_t title_color,
                const char *line1, const char *line2)
{
    static const char *const chips[] = { "any key", "Continue", NULL };

    ui_frame(context, chips);
    ui_round_rect(20, 60, SCREEN_W - 40, 110, COL_PANEL);
    ui_text_centered(title, 80, title_color, 2);
    ui_text_centered(line1, 118, COL_TEXT, 1);
    if (line2)
    {
        ui_text_centered(line2, 134, COL_DIM, 1);
    }
    gfx_SwapDraw();

    ui_wait_key();
}

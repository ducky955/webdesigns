#include "ui.h"

#include <graphx.h>
#include <ti/getcsc.h>
#include <ti/screen.h>

void ui_init(void)
{
    gfx_Begin();

    gfx_palette[COL_BG]     = gfx_RGBTo1555(16, 18, 26);
    gfx_palette[COL_BAR]    = gfx_RGBTo1555(34, 38, 54);
    gfx_palette[COL_TEXT]   = gfx_RGBTo1555(232, 234, 242);
    gfx_palette[COL_DIM]    = gfx_RGBTo1555(128, 136, 160);
    gfx_palette[COL_ACCENT] = gfx_RGBTo1555(58, 128, 246);
    gfx_palette[COL_ERR]    = gfx_RGBTo1555(248, 92, 92);
    gfx_palette[COL_OK]     = gfx_RGBTo1555(92, 208, 132);
    gfx_palette[COL_ASM]    = gfx_RGBTo1555(250, 184, 72);
    gfx_palette[COL_BASIC]  = gfx_RGBTo1555(120, 200, 230);

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

void ui_frame(const char *context, const char *hints)
{
    gfx_FillScreen(COL_BG);

    gfx_SetColor(COL_BAR);
    gfx_FillRectangle_NoClip(0, 0, SCREEN_W, TOP_BAR_H);
    gfx_FillRectangle_NoClip(0, BOTTOM_BAR_Y, SCREEN_W, BOTTOM_BAR_H);
    gfx_SetColor(COL_ACCENT);
    gfx_HorizLine_NoClip(0, TOP_BAR_H, SCREEN_W);

    ui_text("CESecure", 8, 6, COL_TEXT);
    if (context)
    {
        ui_text(context, SCREEN_W - 8 - (int)gfx_GetStringWidth(context), 6, COL_DIM);
    }
    if (hints)
    {
        ui_text_centered(hints, BOTTOM_BAR_Y + 5, COL_DIM, 1);
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
    gfx_SetColor(color);
    gfx_FillRectangle_NoClip(cx - 20, y + 20, 40, 30);

    /* Keyhole */
    gfx_SetColor(COL_BG);
    gfx_FillCircle_NoClip(cx, y + 31, 4);
    gfx_FillRectangle_NoClip(cx - 2, y + 31, 4, 10);
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

void ui_message(const char *context, const char *title, uint8_t title_color,
                const char *line1, const char *line2)
{
    ui_frame(context, "press any key");
    ui_text_centered(title, 80, title_color, 2);
    ui_text_centered(line1, 120, COL_TEXT, 1);
    if (line2)
    {
        ui_text_centered(line2, 136, COL_DIM, 1);
    }
    gfx_SwapDraw();

    ui_wait_key();
}

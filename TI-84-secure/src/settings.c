#include "settings.h"
#include "config.h"
#include "pin.h"
#include "ui.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include <graphx.h>
#include <ti/getcsc.h>

enum { OPT_THEME, OPT_CLOCK, OPT_SORT, OPT_HIDDEN, OPT_PIN, OPT_COUNT };

#define ROW_X   12
#define ROW_W   (SCREEN_W - 2 * ROW_X)
#define ROW_Y   (TOP_BAR_H + 8)
#define ROW_H   22
#define ROW_GAP 2
#define INFO_Y  (ROW_Y + OPT_COUNT * (ROW_H + ROW_GAP) + 4)
#define INFO_H  (BOTTOM_BAR_Y - 6 - INFO_Y)

static const char *const labels[OPT_COUNT] = {
    "Theme", "Clock", "Sort programs by", "Hidden programs", "Change PIN",
};

static const char *const about[OPT_COUNT] = {
    "Color scheme for every screen. Changes show up right away.",
    "How the clock in the top bar shows the time.",
    "Order of the program list. Favorites always stay on top.",
    "Show programs that are hidden from the TI-OS prgm menu.",
    "Asks for your current PIN, then a new one.",
};

static const char *const sort_names[SORT_COUNT] = { "Name", "Type", "Size" };

static const char *option_value(uint8_t option)
{
    switch (option)
    {
        case OPT_THEME:  return ui_theme_name(config.theme);
        case OPT_CLOCK:  return config.clock24 ? "24-hour" : "12-hour";
        case OPT_SORT:   return sort_names[config.sort];
        case OPT_HIDDEN: return config.show_hidden ? "Show" : "Hide";
        default:         return NULL;
    }
}

/* Steps an option forward (+1) or back (-1). */
static void change_option(uint8_t option, int8_t step)
{
    switch (option)
    {
        case OPT_THEME:
            config.theme = (uint8_t)((config.theme + ui_theme_count() + step) % ui_theme_count());
            ui_apply_theme(config.theme);
            break;
        case OPT_CLOCK:
            config.clock24 = !config.clock24;
            break;
        case OPT_SORT:
            config.sort = (uint8_t)((config.sort + SORT_COUNT + step) % SORT_COUNT);
            break;
        case OPT_HIDDEN:
            config.show_hidden = !config.show_hidden;
            break;
        default:
            return;
    }
    config_mark_dirty();
}

static void draw_swatches(int x, int y)
{
    static const uint8_t swatches[] = {
        COL_BG, COL_PANEL, COL_ACCENT, COL_TEXT, COL_TAG_C, COL_TAG_BASIC, COL_WARN, COL_FAV,
    };

    for (uint8_t i = 0; i < sizeof swatches; i++)
    {
        ui_round_rect(x + i * 20 - 1, y - 1, 18, 14, COL_LINE);
        ui_round_rect(x + i * 20, y, 16, 12, swatches[i]);
    }
}

static void draw(uint8_t sel)
{
    static const char *const chips[] = {
        "<>", "Change", "enter", "Select", "clear", "Back", NULL
    };

    ui_frame("Settings", chips);

    for (uint8_t i = 0; i < OPT_COUNT; i++)
    {
        int y = ROW_Y + i * (ROW_H + ROW_GAP);
        bool selected = i == sel;
        uint8_t text = selected ? COL_ON_ACCENT : COL_TEXT;
        const char *value = option_value(i);

        ui_round_rect(ROW_X, y, ROW_W, ROW_H, selected ? COL_ACCENT : COL_PANEL);
        ui_text(labels[i], ROW_X + 10, y + 7, text);

        if (value)
        {
            int w = (int)gfx_GetStringWidth(value);
            int x = ROW_X + ROW_W - 10 - w;

            if (selected)
            {
                ui_text("<", x - 14, y + 7, text);
                ui_text(">", x + w + 4, y + 7, text);
                x -= 2;
            }
            ui_text(value, x, y + 7, selected ? text : COL_DIM);
        }
        else
        {
            ui_text(">", ROW_X + ROW_W - 16, y + 7, selected ? text : COL_DIM);
        }
    }

    ui_round_rect(ROW_X, INFO_Y, ROW_W, INFO_H, COL_PANEL);
    ui_text("About", ROW_X + 10, INFO_Y + 6, COL_ACCENT);
    ui_text_wrapped(about[sel], ROW_X + 10, INFO_Y + 20, ROW_W - 20, 2, COL_TEXT);
    if (sel == OPT_THEME)
    {
        draw_swatches(ROW_X + 10, INFO_Y + INFO_H - 18);
    }

    gfx_SwapDraw();
}

void settings_run(void)
{
    uint8_t sel = 0;

    for (;;)
    {
        draw(sel);

        switch (ui_poll_key())
        {
            case sk_Up:
                sel = sel ? sel - 1 : OPT_COUNT - 1;
                break;
            case sk_Down:
                sel = sel + 1 < OPT_COUNT ? sel + 1 : 0;
                break;
            case sk_Left:
                change_option(sel, -1);
                break;
            case sk_Right:
                change_option(sel, 1);
                break;
            case sk_Enter:
            case sk_2nd:
                if (sel == OPT_PIN)
                {
                    pin_change();
                }
                else
                {
                    change_option(sel, 1);
                }
                break;
            case sk_Clear:
            case sk_Mode:
                config_save();
                return;
            default:
                break;
        }
    }
}

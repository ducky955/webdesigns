#ifndef CESECURE_UI_H
#define CESECURE_UI_H

#include <stdint.h>

/* Palette slots overwritten by the active theme. Everything else keeps the
 * default graphx palette so program icons draw correctly. Index 255 is left
 * alone and used as the text background/transparent color. */
enum {
    COL_BG = 1,     /* screen background */
    COL_PANEL,      /* cards and panels */
    COL_BAR,        /* top and bottom bars */
    COL_TEXT,
    COL_DIM,        /* secondary text */
    COL_ACCENT,     /* selection, highlights */
    COL_ON_ACCENT,  /* text drawn on COL_ACCENT */
    COL_ERR,
    COL_OK,
    COL_WARN,
    COL_TAG_C,
    COL_TAG_BASIC,
    COL_TAG_ICE,
    COL_FAV,
    COL_LINE,       /* dividers, key chips */
};
#define THEME_SLOTS      (COL_LINE - COL_BG + 1)
#define COL_TRANSPARENT  255

#define SCREEN_W      320
#define SCREEN_H      240
#define TOP_BAR_H     20
#define BOTTOM_BAR_H  18
#define BOTTOM_BAR_Y  (SCREEN_H - BOTTOM_BAR_H)

/* Starts graphx, loads the theme from config and selects the back buffer.
 * Also used as the "after garbage collection" handler. */
void ui_init(void);

/* Stops graphx (restores the OS 16bpp LCD mode). */
void ui_end(void);

/* Stops graphx and leaves a clean homescreen for the OS. */
void ui_quit(void);

uint8_t ui_theme_count(void);
const char *ui_theme_name(uint8_t theme);

/* Writes a theme's colors into the palette (takes effect immediately). */
void ui_apply_theme(uint8_t theme);

/* Maps a default-palette icon pixel away from the slots the theme uses. */
uint8_t ui_icon_pixel(uint8_t index);

/* Clears the back buffer and draws the top bar (title, context, clock,
 * battery) and a bottom bar of key chips. chips is a NULL-terminated list
 * of alternating key and label strings, or NULL for an empty bar. */
void ui_frame(const char *context, const char *const *chips);

void ui_text(const char *s, int x, int y, uint8_t color);
void ui_text_centered(const char *s, int y, uint8_t color, uint8_t scale);

/* Draws s word-wrapped inside width w; returns the number of lines used. */
uint8_t ui_text_wrapped(const char *s, int x, int y, int w, uint8_t max_lines, uint8_t color);

/* Filled rectangle with 2px rounded corners. */
void ui_round_rect(int x, int y, int w, int h, uint8_t color);

/* Draws a padlock whose shackle top sits at y, centered on cx. */
void ui_draw_lock(int cx, int y, uint8_t color);

/* Draws a small 7x7 star with its top-left corner at x, y. */
void ui_draw_star(int x, int y, uint8_t color);

/* Small string builders (avoids linking printf). Both write at dst, add a
 * NUL, and return a pointer to that NUL so calls can be chained. */
char *ui_append(char *dst, const char *s);
char *ui_append_uint(char *dst, unsigned int n);

/* Waits for a key press and returns its sk_* scan code. */
uint8_t ui_wait_key(void);

/* Like ui_wait_key, but returns 0 when the clock's minute changes so the
 * caller can redraw the top bar. */
uint8_t ui_poll_key(void);

/* Shows a full-screen message and waits for any key. line2 may be NULL. */
void ui_message(const char *context, const char *title, uint8_t title_color,
                const char *line1, const char *line2);

#endif

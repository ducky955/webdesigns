#ifndef CESECURE_UI_H
#define CESECURE_UI_H

#include <stdint.h>

/* Palette slots overwritten in ui_init(). Index 255 is left alone and used
 * as the text background/transparent color. */
enum {
    COL_BG = 1,
    COL_BAR,
    COL_TEXT,
    COL_DIM,
    COL_ACCENT,
    COL_ERR,
    COL_OK,
    COL_ASM,
    COL_BASIC,
};
#define COL_TRANSPARENT 255

#define SCREEN_W      320
#define SCREEN_H      240
#define TOP_BAR_H     20
#define BOTTOM_BAR_H  18
#define BOTTOM_BAR_Y  (SCREEN_H - BOTTOM_BAR_H)

/* Starts graphx, loads the dark palette and selects the back buffer. Also
 * used as the "after garbage collection" handler. */
void ui_init(void);

/* Stops graphx (restores the OS 16bpp LCD mode). */
void ui_end(void);

/* Stops graphx and leaves a clean homescreen for the OS. */
void ui_quit(void);

/* Clears the back buffer and draws the top bar ("CESecure" + context) and
 * the bottom bar with key hints. */
void ui_frame(const char *context, const char *hints);

void ui_text(const char *s, int x, int y, uint8_t color);
void ui_text_centered(const char *s, int y, uint8_t color, uint8_t scale);

/* Draws a padlock whose shackle top sits at y, centered on cx. */
void ui_draw_lock(int cx, int y, uint8_t color);

/* Small string builders (avoids linking printf). Both write at dst, add a
 * NUL, and return a pointer to that NUL so calls can be chained. */
char *ui_append(char *dst, const char *s);
char *ui_append_uint(char *dst, unsigned int n);

/* Waits for a key press and returns its sk_* scan code. */
uint8_t ui_wait_key(void);

/* Shows a full-screen message and waits for any key. line2 may be NULL. */
void ui_message(const char *context, const char *title, uint8_t title_color,
                const char *line1, const char *line2);

#endif

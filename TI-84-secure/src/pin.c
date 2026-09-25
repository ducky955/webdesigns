#include "pin.h"
#include "ui.h"

#include <stdint.h>
#include <string.h>

#include <fileioc.h>
#include <graphx.h>
#include <sys/rtc.h>
#include <sys/timers.h>
#include <sys/util.h>
#include <ti/getcsc.h>

#define PIN_APPVAR       "CESecPW"
#define PIN_MAGIC        "CSP1"
#define PIN_MIN          4
#define PIN_MAX          8
#define SALT_LEN         8
#define HASH_ROUNDS      2000
#define MAX_TRIES        3
#define LOCKOUT_SECONDS  30

#define FNV_OFFSET  UINT32_C(2166136261)
#define FNV_PRIME   UINT32_C(16777619)

/* On-calc record. The PIN itself is never stored, only its salted hash. */
typedef struct {
    char magic[4];
    uint8_t salt[SALT_LEN];
    uint32_t hash;
} pin_record_t;

_Static_assert(sizeof(pin_record_t) == 16, "pin_record_t must be packed");

enum { SAVE_OK, SAVE_RAM_ONLY, SAVE_FAILED };

static uint32_t fnv1a(uint32_t h, const uint8_t *data, uint8_t len)
{
    while (len--)
    {
        h ^= *data++;
        h *= FNV_PRIME;
    }
    return h;
}

/* Salted, iterated FNV-1a. Each round folds the salt and PIN into the
 * running state, so the cost grows with HASH_ROUNDS. */
static uint32_t pin_hash(const uint8_t salt[SALT_LEN], const char *pin, uint8_t len)
{
    uint32_t h = FNV_OFFSET;

    for (uint16_t round = 0; round < HASH_ROUNDS; round++)
    {
        h = fnv1a(h, salt, SALT_LEN);
        h = fnv1a(h, (const uint8_t *)pin, len);
    }
    return h;
}

static bool pin_load(pin_record_t *rec)
{
    uint8_t handle = ti_Open(PIN_APPVAR, "r");
    bool ok;

    if (!handle)
    {
        return false;
    }
    ok = ti_GetSize(handle) == sizeof *rec &&
         ti_Read(rec, sizeof *rec, 1, handle) == 1 &&
         memcmp(rec->magic, PIN_MAGIC, sizeof rec->magic) == 0;
    ti_Close(handle);

    return ok;
}

static uint8_t pin_save(const pin_record_t *rec)
{
    uint8_t handle;
    int archived;

    handle = ti_Open(PIN_APPVAR, "w");
    if (!handle)
    {
        return SAVE_FAILED;
    }
    if (ti_Write(rec, sizeof *rec, 1, handle) != 1)
    {
        ti_Close(handle);
        ti_Delete(PIN_APPVAR);
        return SAVE_FAILED;
    }

    /* Archiving can trigger a garbage collect, which draws an OS screen, so
     * drop out of graphx around it. Restore the default handlers afterwards
     * so nothing points at this program once another one runs. */
    ti_SetGCBehavior(ui_end, ui_init);
    archived = ti_SetArchiveStatus(true, handle);
    ti_SetGCBehavior(NULL, NULL);
    ti_Close(handle);

    return archived ? SAVE_OK : SAVE_RAM_ONLY;
}

bool pin_is_set(void)
{
    pin_record_t rec;

    return pin_load(&rec);
}

static int8_t key_to_digit(uint8_t key)
{
    switch (key)
    {
        case sk_0: return 0;
        case sk_1: return 1;
        case sk_2: return 2;
        case sk_3: return 3;
        case sk_4: return 4;
        case sk_5: return 5;
        case sk_6: return 6;
        case sk_7: return 7;
        case sk_8: return 8;
        case sk_9: return 9;
        default:   return -1;
    }
}

static void draw_dots(uint8_t len, int y)
{
    /* Filled dots for entered digits, hollow ones up to the minimum length. */
    uint8_t slots = len > PIN_MIN ? len : PIN_MIN;
    int x = SCREEN_W / 2 - (slots - 1) * 11;

    for (uint8_t i = 0; i < slots; i++, x += 22)
    {
        if (i < len)
        {
            gfx_SetColor(COL_TEXT);
            gfx_FillCircle_NoClip(x, y, 6);
        }
        else
        {
            gfx_SetColor(COL_DIM);
            gfx_Circle(x, y, 6);
        }
    }
}

/* Reads a 4-8 digit PIN into pin (NUL-terminated). Returns its length, or 0
 * if the user pressed [clear]. */
static uint8_t pin_entry(const char *context, const char *title, const char *subtitle,
                         const char *msg, uint8_t msg_color, const char *cancel_label,
                         char pin[PIN_MAX + 1])
{
    char hints[40];
    uint8_t len = 0;

    ui_append(ui_append(hints, "del:erase  enter:OK  clear:"), cancel_label);

    for (;;)
    {
        uint8_t key;
        int8_t digit;

        ui_frame(context, hints);
        ui_draw_lock(SCREEN_W / 2, 36, COL_ACCENT);
        ui_text_centered(title, 100, COL_TEXT, 2);
        if (subtitle)
        {
            ui_text_centered(subtitle, 122, COL_DIM, 1);
        }
        draw_dots(len, 150);
        if (msg)
        {
            ui_text_centered(msg, 176, msg_color, 1);
        }
        gfx_SwapDraw();

        key = ui_wait_key();
        digit = key_to_digit(key);

        if (digit >= 0)
        {
            if (len < PIN_MAX)
            {
                pin[len++] = (char)('0' + digit);
            }
            msg = NULL;
        }
        else if (key == sk_Del)
        {
            if (len > 0)
            {
                len--;
            }
            msg = NULL;
        }
        else if (key == sk_Enter)
        {
            if (len >= PIN_MIN)
            {
                pin[len] = '\0';
                return len;
            }
            msg = "PIN must be 4-8 digits";
            msg_color = COL_ERR;
        }
        else if (key == sk_Clear)
        {
            memset(pin, 0, PIN_MAX + 1);
            return 0;
        }
    }
}

static void lockout(const char *context)
{
    char line[32];

    for (uint8_t left = LOCKOUT_SECONDS; left > 0; left--)
    {
        ui_frame(context, "locked");
        ui_draw_lock(SCREEN_W / 2, 36, COL_ERR);
        ui_text_centered("Locked", 100, COL_ERR, 2);
        ui_text_centered("Too many wrong PINs", 128, COL_TEXT, 1);
        ui_append(ui_append_uint(ui_append(line, "Try again in "), left), " s");
        ui_text_centered(line, 150, COL_DIM, 1);
        gfx_SwapDraw();
        msleep(1000);
    }

    /* Drop anything pressed during the lockout. */
    while (os_GetCSC());
}

static bool verify(const pin_record_t *rec, const char *context, const char *cancel_label)
{
    char pin[PIN_MAX + 1];
    char msg[32];
    const char *shown = NULL;
    uint8_t fails = 0;

    for (;;)
    {
        uint8_t len = pin_entry(context, "Enter PIN", NULL, shown, COL_ERR, cancel_label, pin);
        bool ok;

        if (!len)
        {
            return false;
        }
        ok = pin_hash(rec->salt, pin, len) == rec->hash;
        memset(pin, 0, sizeof pin);
        if (ok)
        {
            return true;
        }

        if (++fails >= MAX_TRIES)
        {
            lockout(context);
            fails = 0;
            shown = NULL;
        }
        else
        {
            uint8_t left = MAX_TRIES - fails;
            ui_append(ui_append_uint(ui_append(msg, "Wrong PIN - "), left),
                      left == 1 ? " try left" : " tries left");
            shown = msg;
        }
    }
}

bool pin_unlock(const char *context, const char *cancel_label)
{
    pin_record_t rec;

    if (!pin_load(&rec))
    {
        return false;
    }
    return verify(&rec, context, cancel_label);
}

bool pin_setup(const char *context, const char *cancel_label)
{
    char first[PIN_MAX + 1];
    char second[PIN_MAX + 1];
    const char *msg = NULL;
    pin_record_t rec;
    uint8_t len;
    uint8_t status;

    for (;;)
    {
        uint8_t len2;

        len = pin_entry(context, "New PIN", "Choose 4 to 8 digits", msg, COL_ERR, cancel_label, first);
        if (!len)
        {
            return false;
        }
        len2 = pin_entry(context, "Confirm PIN", "Enter it again", NULL, COL_ERR, cancel_label, second);
        if (!len2)
        {
            memset(first, 0, sizeof first);
            return false;
        }
        if (len == len2 && memcmp(first, second, len) == 0)
        {
            break;
        }
        msg = "PINs did not match - try again";
    }

    ui_frame(context, NULL);
    ui_text_centered("Saving...", 112, COL_TEXT, 2);
    gfx_SwapDraw();

    /* Seed here rather than in main: after a launched program returns, the
     * launcher restarts in the os_RunPrgm callback and main never runs. */
    srandom(rtc_Time());
    memcpy(rec.magic, PIN_MAGIC, sizeof rec.magic);
    for (uint8_t i = 0; i < SALT_LEN; i++)
    {
        rec.salt[i] = (uint8_t)random();
    }
    rec.hash = pin_hash(rec.salt, first, len);
    memset(first, 0, sizeof first);
    memset(second, 0, sizeof second);

    status = pin_save(&rec);
    if (status == SAVE_FAILED)
    {
        ui_message(context, "Error", COL_ERR, "Could not save the PIN.", "Free some RAM and try again.");
        return false;
    }
    if (status == SAVE_RAM_ONLY)
    {
        ui_message(context, "PIN saved", COL_ASM, "Archive is full, so the PIN is in RAM.",
                   "A RAM reset will remove it.");
    }
    else
    {
        ui_message(context, "PIN saved", COL_OK, "Your new PIN is active.", NULL);
    }
    return true;
}

void pin_change(void)
{
    if (!pin_unlock("Change PIN", "back"))
    {
        return;
    }
    pin_setup("Change PIN", "back");
}

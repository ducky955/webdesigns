#ifndef CESECURE_PIN_H
#define CESECURE_PIN_H

#include <stdbool.h>

/* True if a valid PIN record exists in the CESecPW AppVar. */
bool pin_is_set(void);

/* Asks for a new PIN twice and saves it. Returns false if the user pressed
 * [clear] or the PIN could not be saved. cancel_label names what [clear]
 * does on this screen ("quit" or "back"). */
bool pin_setup(const char *context, const char *cancel_label);

/* Asks for the stored PIN until it is entered correctly, enforcing the
 * 3-strike lockout. Returns false if the user pressed [clear]. */
bool pin_unlock(const char *context, const char *cancel_label);

/* Launcher [mode] action: verify the current PIN, then set a new one. */
void pin_change(void);

#endif

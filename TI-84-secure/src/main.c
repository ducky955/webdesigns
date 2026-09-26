/*
 * CESecure - PIN-locked program launcher for the TI-84 Plus CE.
 *
 * First launch asks for a new PIN; later launches ask for it before showing
 * the program list. See README.md for what this does and does not protect.
 */

#include "config.h"
#include "launcher.h"
#include "pin.h"
#include "ui.h"

#include <stddef.h>

int main(void)
{
    bool unlocked;

    config_load();
    ui_init();

    if (pin_is_set())
    {
        unlocked = pin_unlock("Unlock", "Quit");
    }
    else
    {
        unlocked = pin_setup("First run", "Quit");
    }

    if (unlocked)
    {
        launcher_run(NULL);
        config_save();
    }

    ui_quit();
    return 0;
}

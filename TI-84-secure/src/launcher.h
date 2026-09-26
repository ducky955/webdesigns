#ifndef CESECURE_LAUNCHER_H
#define CESECURE_LAUNCHER_H

/* Runs the program list until the user presses [clear]. Graphics must
 * already be initialized. If reselect names a program in the list, the
 * cursor starts on it. */
void launcher_run(const char *reselect);

#ifdef CESECURE_APP
#include <stdbool.h>

/* App build: if a program was just launched from the app, copies its name
 * into name (9 bytes) and returns true. Consumes the marker. */
bool launcher_take_resume(char name[9]);
#endif

#endif

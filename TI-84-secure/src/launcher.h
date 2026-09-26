#ifndef CESECURE_LAUNCHER_H
#define CESECURE_LAUNCHER_H

/* Runs the program list until the user presses [clear]. Graphics must
 * already be initialized. If reselect names a program in the list, the
 * cursor starts on it. */
void launcher_run(const char *reselect);

#endif

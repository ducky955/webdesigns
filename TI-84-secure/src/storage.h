#ifndef CESECURE_STORAGE_H
#define CESECURE_STORAGE_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

enum { STORE_OK, STORE_RAM_ONLY, STORE_FAILED };

/* Replaces the AppVar with data and archives it. Returns STORE_RAM_ONLY if
 * the write worked but archiving did not (archive full). */
uint8_t store_write(const char *name, const void *data, size_t size);

/* Reads the AppVar into data. Fails unless it is exactly size bytes. */
bool store_read(const char *name, void *data, size_t size);

#endif

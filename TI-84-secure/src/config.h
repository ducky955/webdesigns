#ifndef CESECURE_CONFIG_H
#define CESECURE_CONFIG_H

#include <stdbool.h>
#include <stdint.h>

#define FAV_MAX        16
#define PRGM_NAME_MAX  8

enum { SORT_NAME, SORT_TYPE, SORT_SIZE, SORT_COUNT };

/* Saved in the CESecCfg AppVar. Names are zero-padded, not NUL-terminated. */
typedef struct {
    char magic[4];
    uint8_t theme;
    uint8_t clock24;
    uint8_t show_hidden;
    uint8_t sort;
    uint8_t fav_count;
    char favs[FAV_MAX][PRGM_NAME_MAX];
} config_t;

extern config_t config;

/* Loads settings, falling back to defaults if the AppVar is missing. */
void config_load(void);

/* Call after changing a field; config_save() only writes when needed. */
void config_mark_dirty(void);
void config_save(void);

bool config_is_favorite(const char *name);

/* Adds or removes a favorite. Returns false if the list is full. */
bool config_toggle_favorite(const char *name);

#endif

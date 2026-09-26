#include "config.h"
#include "storage.h"

#include <string.h>

#define CONFIG_APPVAR  "CESecCfg"
#define CONFIG_MAGIC   "CSC1"

config_t config;
static bool dirty;

void config_load(void)
{
    dirty = false;
    if (store_read(CONFIG_APPVAR, &config, sizeof config) &&
        memcmp(config.magic, CONFIG_MAGIC, sizeof config.magic) == 0)
    {
        /* Clamp anything a newer or damaged file might hold. */
        if (config.sort >= SORT_COUNT)
        {
            config.sort = SORT_NAME;
        }
        if (config.fav_count > FAV_MAX)
        {
            config.fav_count = 0;
        }
        return;
    }

    memset(&config, 0, sizeof config);
    memcpy(config.magic, CONFIG_MAGIC, sizeof config.magic);
}

void config_mark_dirty(void)
{
    dirty = true;
}

void config_save(void)
{
    if (dirty)
    {
        store_write(CONFIG_APPVAR, &config, sizeof config);
        dirty = false;
    }
}

static int find_favorite(const char *name)
{
    for (uint8_t i = 0; i < config.fav_count; i++)
    {
        if (strncmp(config.favs[i], name, PRGM_NAME_MAX) == 0)
        {
            return i;
        }
    }
    return -1;
}

bool config_is_favorite(const char *name)
{
    return find_favorite(name) >= 0;
}

bool config_toggle_favorite(const char *name)
{
    int index = find_favorite(name);

    if (index >= 0)
    {
        config.fav_count--;
        memmove(config.favs[index], config.favs[index + 1],
                (size_t)(config.fav_count - index) * PRGM_NAME_MAX);
        memset(config.favs[config.fav_count], 0, PRGM_NAME_MAX);
    }
    else
    {
        size_t len = strlen(name);

        if (config.fav_count >= FAV_MAX)
        {
            return false;
        }
        memset(config.favs[config.fav_count], 0, PRGM_NAME_MAX);
        memcpy(config.favs[config.fav_count], name, len < PRGM_NAME_MAX ? len : PRGM_NAME_MAX);
        config.fav_count++;
    }

    dirty = true;
    return true;
}

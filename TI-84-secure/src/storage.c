#include "storage.h"
#include "ui.h"

#include <fileioc.h>

uint8_t store_write(const char *name, const void *data, size_t size)
{
    uint8_t handle;
    int archived;

    handle = ti_Open(name, "w");
    if (!handle)
    {
        return STORE_FAILED;
    }
    if (ti_Write(data, size, 1, handle) != 1)
    {
        ti_Close(handle);
        ti_Delete(name);
        return STORE_FAILED;
    }

    /* Archiving can trigger a garbage collect, which draws an OS screen, so
     * drop out of graphx around it. Restore the default handlers afterwards
     * so nothing points at this program once another one runs. */
    ti_SetGCBehavior(ui_end, ui_init);
    archived = ti_SetArchiveStatus(true, handle);
    ti_SetGCBehavior(NULL, NULL);
    ti_Close(handle);

    return archived ? STORE_OK : STORE_RAM_ONLY;
}

bool store_read(const char *name, void *data, size_t size)
{
    uint8_t handle = ti_Open(name, "r");
    bool ok;

    if (!handle)
    {
        return false;
    }
    ok = ti_GetSize(handle) == size && ti_Read(data, size, 1, handle) == 1;
    ti_Close(handle);

    return ok;
}

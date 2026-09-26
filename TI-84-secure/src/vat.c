#include "vat.h"
#include "ui.h"

#include <string.h>

#include <fileioc.h>
#include <ti/vars.h>

bool vat_is_hidden_name(const char *raw)
{
    char shown = (char)(raw[0] | VAT_HIDDEN_BIT);

    return !(raw[0] & VAT_HIDDEN_BIT) && shown >= 'A' && shown <= '[';
}

void vat_display_name(const char *raw, char out[VAT_NAME_SIZE])
{
    size_t len = strlen(raw);

    if (len >= VAT_NAME_SIZE)
    {
        len = VAT_NAME_SIZE - 1;
    }
    memcpy(out, raw, len);
    out[len] = '\0';
    if (vat_is_hidden_name(out))
    {
        out[0] = (char)(out[0] | VAT_HIDDEN_BIT);
    }
}

/* Looks up one exact spelling as either program type. */
static bool find_exact(const char *name, vat_program_t *out)
{
    static const uint8_t types[] = { OS_TYPE_PRGM, OS_TYPE_PROT_PRGM };

    for (uint8_t i = 0; i < sizeof types; i++)
    {
        uint8_t handle = ti_OpenVar(name, "r", types[i]);

        if (handle)
        {
            size_t len = strlen(name);

            memcpy(out->name, name, len + 1);
            out->type = types[i];
            out->archived = ti_IsArchived(handle) != 0;
            out->size = ti_GetSize(handle);
            ti_Close(handle);
            return true;
        }
    }
    return false;
}

/* Returns name with the first character's hidden bit flipped. */
static void flip_hidden_bit(const char *name, char out[VAT_NAME_SIZE])
{
    size_t len = strlen(name);

    memcpy(out, name, len + 1);
    out[0] = (char)(out[0] ^ VAT_HIDDEN_BIT);
}

bool vat_find_program(const char *name, vat_program_t *out)
{
    char other[VAT_NAME_SIZE];

    if (!name[0] || strlen(name) >= VAT_NAME_SIZE)
    {
        return false;
    }
    if (find_exact(name, out))
    {
        return true;
    }
    flip_hidden_bit(name, other);
    return find_exact(other, out);
}

uint8_t vat_set_hidden(const char *name, bool hidden)
{
    vat_program_t prgm;
    char new_name[VAT_NAME_SIZE];
    uint8_t result;

    if (!vat_find_program(name, &prgm))
    {
        return VAT_NOT_FOUND;
    }
    if (vat_is_hidden_name(prgm.name) == hidden)
    {
        return VAT_OK;
    }

    /* The VAT name and, for archived programs, the copy of the name in
     * flash both have to change, or the old name comes back when the VAT
     * is rebuilt. ti_RenameVar rewrites the program under the new name and
     * keeps it in archive if it was there. It needs a RAM copy first and
     * doesn't check for room itself. */
    if (os_MemChk(NULL) < (size_t)prgm.size + 64)
    {
        return VAT_NO_MEMORY;
    }

    flip_hidden_bit(prgm.name, new_name);

    /* Renaming may archive and trigger a garbage collect. */
    ti_SetGCBehavior(ui_end, ui_init);
    result = ti_RenameVar(prgm.name, new_name, prgm.type);
    ti_SetGCBehavior(NULL, NULL);

    switch (result)
    {
        case 0:  return VAT_OK;
        case 1:  return VAT_NAME_TAKEN;
        case 2:  return VAT_NOT_FOUND;
        default: return VAT_FAILED;
    }
}

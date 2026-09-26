#ifndef CESECURE_VAT_H
#define CESECURE_VAT_H

#include <stdbool.h>
#include <stdint.h>

/* Program names are at most 8 characters. */
#define VAT_NAME_SIZE 9

/* TI-OS hides a program by clearing bit 6 of the first character of its
 * name ('A' 0x41 -> 0x01). Every valid first character ('A'-'Z' and theta,
 * 0x41-0x5B) has this bit set, so the bit being clear means "hidden". */
#define VAT_HIDDEN_BIT 0x40

enum {
    VAT_OK,
    VAT_NOT_FOUND,
    VAT_NO_MEMORY,    /* not enough free RAM to rewrite the program */
    VAT_NAME_TAKEN,   /* a program with the other spelling already exists */
    VAT_FAILED,
};

typedef struct {
    char name[VAT_NAME_SIZE];     /* name exactly as stored in the VAT */
    uint8_t type;                 /* OS_TYPE_PRGM or OS_TYPE_PROT_PRGM */
    bool archived;
    uint16_t size;
} vat_program_t;

/* True if raw (a VAT name) is a hidden program name. */
bool vat_is_hidden_name(const char *raw);

/* Copies raw into out with the hidden bit restored, so it reads normally. */
void vat_display_name(const char *raw, char out[VAT_NAME_SIZE]);

/* Finds a program by name, whether name is given in its hidden or visible
 * spelling and whatever the program's current state. An exact match wins. */
bool vat_find_program(const char *name, vat_program_t *out);

/* Hides or unhides a program (either spelling of name works). Archived
 * programs stay archived. Returns a VAT_* code; VAT_OK if already so. */
uint8_t vat_set_hidden(const char *name, bool hidden);

#endif

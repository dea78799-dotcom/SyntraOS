#ifndef SYNTRA_ARM64_BOOT_FDT_H
#define SYNTRA_ARM64_BOOT_FDT_H

#include <stddef.h>
#include <stdint.h>

#include "../../core/status.h"

#define SYNTRA_FDT_MAGIC 0xD00DFEEDU
#define SYNTRA_FDT_MAX_VERSION 17U

typedef struct
{
    const uint8_t *blob;
    size_t total_size;
    const uint8_t *structure;
    size_t structure_size;
    const uint8_t *strings;
    size_t strings_size;
} syntra_fdt_t;

typedef enum
{
    SYNTRA_FDT_TOKEN_BEGIN_NODE = 1,
    SYNTRA_FDT_TOKEN_END_NODE = 2,
    SYNTRA_FDT_TOKEN_PROPERTY = 3,
    SYNTRA_FDT_TOKEN_NOP = 4,
    SYNTRA_FDT_TOKEN_END = 9
} syntra_fdt_token_type_t;

typedef struct
{
    size_t offset;
    uint32_t depth;
    uint8_t ended;
} syntra_fdt_iterator_t;

typedef struct
{
    syntra_fdt_token_type_t type;
    uint32_t depth;
    size_t offset;
    const char *name;
    const uint8_t *data;
    uint32_t data_size;
} syntra_fdt_token_t;

typedef struct
{
    size_t begin_offset;
    uint32_t depth;
} syntra_fdt_node_t;

/* Open and validate an FDT blob within the supplied accessible byte range. */
syntra_status_t syntra_fdt_open(
    const void *blob,
    size_t accessible_size,
    syntra_fdt_t *fdt
);

/* Initialize an iterator at the first structure-block token. */
void syntra_fdt_iterator_init(syntra_fdt_iterator_t *iterator);

/*
 * Read the next structure token.
 * Returns SYNTRA_STATUS_OK for a token, SYNTRA_STATUS_NOT_IMPLEMENTED
 * after the FDT_END token, and SYNTRA_STATUS_ERROR for malformed data.
 */
syntra_status_t syntra_fdt_next(
    const syntra_fdt_t *fdt,
    syntra_fdt_iterator_t *iterator,
    syntra_fdt_token_t *token
);

/* Find the first node whose compatible string list contains the requested value. */
syntra_status_t syntra_fdt_find_compatible(
    const syntra_fdt_t *fdt,
    const char *compatible,
    syntra_fdt_node_t *node
);

/* Find a property belonging directly to a node, not one of its children. */
syntra_status_t syntra_fdt_get_property(
    const syntra_fdt_t *fdt,
    const syntra_fdt_node_t *node,
    const char *property_name,
    const uint8_t **data,
    uint32_t *data_size
);

#endif

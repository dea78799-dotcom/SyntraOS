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
    const uint8_t *reserve_map;
    size_t reserve_map_size;
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

/* Resolve a node by its standard phandle or legacy linux,phandle property. */
syntra_status_t syntra_fdt_find_phandle(
    const syntra_fdt_t *fdt,
    uint32_t phandle,
    syntra_fdt_node_t *node
);

/*
 * Find a property belonging directly to a node, not one of its children.
 * Returns SYNTRA_STATUS_NOT_IMPLEMENTED when the property is absent.
 */
syntra_status_t syntra_fdt_get_property(
    const syntra_fdt_t *fdt,
    const syntra_fdt_node_t *node,
    const char *property_name,
    const uint8_t **data,
    uint32_t *data_size
);

/* Count validated FDT header reservation-map entries (terminator excluded). */
syntra_status_t syntra_fdt_get_reservation_count(
    const syntra_fdt_t *fdt,
    size_t *reservation_count
);

/* Retrieve one 64-bit address/size reservation-map entry. */
syntra_status_t syntra_fdt_get_reservation(
    const syntra_fdt_t *fdt,
    size_t index,
    uint64_t *address,
    uint64_t *size
);

/* Count validated reg tuples using the parent's address/size cell widths. */
syntra_status_t syntra_fdt_get_reg_count(
    const syntra_fdt_t *fdt,
    const syntra_fdt_node_t *node,
    size_t *reg_count
);

/* Resolve one entry from a node's reg property using its parent's cell widths. */
syntra_status_t syntra_fdt_get_reg(
    const syntra_fdt_t *fdt,
    const syntra_fdt_node_t *node,
    uint32_t reg_index,
    uintptr_t *base,
    size_t *size
);

#endif

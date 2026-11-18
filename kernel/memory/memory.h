
#ifndef SYNTRA_MEMORY_H
#define SYNTRA_MEMORY_H

#include "../core/status.h"

#define SYNTRA_MEMORY_BLOCK_SIZE 4096
#define SYNTRA_MAX_MEMORY_BLOCKS 1024

#define SYNTRA_MEMORY_POOL_SIZE \
    (SYNTRA_MEMORY_BLOCK_SIZE * SYNTRA_MAX_MEMORY_BLOCKS)

#define SYNTRA_MEMORY_ALIGNMENT 4096
#define SYNTRA_MEMORY_HISTORY_SIZE 128
#define SYNTRA_MEMORY_TAG_LENGTH 32

typedef struct
{
    unsigned int total_blocks;
    unsigned int used_blocks;
    unsigned int free_blocks;

    unsigned int allocation_count;
    unsigned int release_count;
    unsigned int active_allocations;
    unsigned int failed_allocations;

    unsigned int largest_free_block;
    unsigned int free_regions;
    unsigned int fragmentation_blocks;
    unsigned int usage_percent;

    unsigned int integrity_errors;
    unsigned int validation_errors;

    unsigned int peak_used_blocks;
    unsigned int peak_usage_percent;

    unsigned int leak_count;
    unsigned int leak_blocks;

} syntra_memory_info_t;

typedef enum
{
    SYNTRA_MEMORY_EVENT_ALLOCATE = 1,
    SYNTRA_MEMORY_EVENT_RELEASE = 2,
    SYNTRA_MEMORY_EVENT_FAILED = 3

} syntra_memory_event_type_t;

typedef struct
{
    syntra_memory_event_type_t type;

    unsigned int start_block;
    unsigned int block_count;
    unsigned int sequence;

    int valid;

} syntra_memory_history_t;

typedef struct
{
    void *address;
    unsigned int start_block;
    unsigned int block_count;
    int active;

} syntra_memory_allocation_t;

/* Memory snapshot */

typedef struct
{
    unsigned int total_blocks;
    unsigned int used_blocks;
    unsigned int free_blocks;

    unsigned int active_allocations;
    unsigned int allocation_count;
    unsigned int release_count;
    unsigned int failed_allocations;

    unsigned int usage_percent;
    unsigned int peak_used_blocks;

    unsigned int snapshot_sequence;
    int valid;

} syntra_memory_snapshot_t;

/* Memory report */

typedef struct
{
    unsigned int total_snapshots;
    unsigned int total_allocations;
    unsigned int total_releases;
    unsigned int total_failures;

    unsigned int current_used_blocks;
    unsigned int current_free_blocks;
    unsigned int peak_used_blocks;

    unsigned int current_usage_percent;
    unsigned int integrity_errors;
    unsigned int validation_errors;

    int valid;

} syntra_memory_report_t;

/* Allocation tags */

typedef struct
{
    unsigned int allocation_index;
    char name[SYNTRA_MEMORY_TAG_LENGTH];

    int active;

} syntra_memory_tag_t;

/* Memory diagnostics */

typedef enum
{
    SYNTRA_MEMORY_DIAGNOSTIC_NONE = 0,
    SYNTRA_MEMORY_DIAGNOSTIC_UNINITIALIZED = 1,
    SYNTRA_MEMORY_DIAGNOSTIC_INVALID_ALLOCATION = 2,
    SYNTRA_MEMORY_DIAGNOSTIC_INVALID_RELEASE = 3,
    SYNTRA_MEMORY_DIAGNOSTIC_INTEGRITY_FAILURE = 4,
    SYNTRA_MEMORY_DIAGNOSTIC_CAPACITY_WARNING = 5

} syntra_memory_diagnostic_type_t;

typedef struct
{
    syntra_memory_diagnostic_type_t type;

    unsigned int error_count;
    unsigned int used_blocks;
    unsigned int free_blocks;

    int has_warning;
    int valid;

} syntra_memory_diagnostics_t;

/* Core memory API */

syntra_status_t memory_init(void);

void *memory_allocate(unsigned int blocks);

syntra_status_t memory_release(
    void *address,
    unsigned int blocks
);

void memory_get_info(
    syntra_memory_info_t *info
);

unsigned int memory_get_allocation_count(void);

const syntra_memory_allocation_t *memory_get_allocation(
    unsigned int index
);

syntra_status_t memory_validate(void);

syntra_status_t memory_check_integrity(void);

/* Memory history */

unsigned int memory_get_history_count(void);

const syntra_memory_history_t *memory_get_history(
    unsigned int index
);

/* Memory snapshot API */

syntra_status_t memory_create_snapshot(void);

unsigned int memory_get_snapshot_count(void);

const syntra_memory_snapshot_t *memory_get_snapshot(
    unsigned int index
);

/* Memory report API */

syntra_status_t memory_generate_report(
    syntra_memory_report_t *report
);

/* Allocation tag API */

syntra_status_t memory_set_tag(
    unsigned int allocation_index,
    const char *name
);

const syntra_memory_tag_t *memory_get_tag(
    unsigned int allocation_index
);

/* Memory diagnostics API */

syntra_status_t memory_run_diagnostics(
    syntra_memory_diagnostics_t *diagnostics
);

#endif

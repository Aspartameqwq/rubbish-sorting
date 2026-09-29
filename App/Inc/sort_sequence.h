#ifndef SORT_SEQUENCE_H
#define SORT_SEQUENCE_H

#include <stdbool.h>
#include <stdint.h>

#define SORT_SEQUENCE_LENGTH 7U
#define SORT_SEQUENCE_DEFAULT_INTERVAL_MS 5000U
#define SORT_SEQUENCE_MIN_INTERVAL_MS 1000U
#define SORT_SEQUENCE_MAX_INTERVAL_MS 60000U

typedef enum
{
    SORT_SEQUENCE_DISABLED = 0,
    SORT_SEQUENCE_WAIT_READY,
    SORT_SEQUENCE_RUNNING,
    SORT_SEQUENCE_WAIT_INTERVAL,
    SORT_SEQUENCE_COMPLETE,
    SORT_SEQUENCE_FAULT,
    SORT_SEQUENCE_INVALID_INTERVAL
} SortSequenceStatus;

typedef struct
{
    /* Ozone inputs: set enabled to 1 to run, 0 to cancel future actions. */
    uint32_t enabled;
    uint32_t interval_ms;
    /* Ozone outputs: do not edit these fields. next_index is zero-based. */
    uint32_t status;
    uint32_t next_index;
    uint32_t completed_count;
    uint32_t active_box;
    uint32_t last_result;
    uint32_t last_completion_tick_ms;
} SortSequenceDebug;

extern volatile SortSequenceDebug g_sort_sequence;

void SortSequence_Init(void);
void SortSequence_Process(uint32_t now_ms);
bool SortSequence_IsEnabled(void);

#endif /* SORT_SEQUENCE_H */

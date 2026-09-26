#ifndef TB6600_TIMING_H
#define TB6600_TIMING_H

#include <stdint.h>

typedef enum
{
    TB6600_TIMING_STATUS_OK = 0,
    TB6600_TIMING_STATUS_INVALID_ARGUMENT,
    TB6600_TIMING_STATUS_OUT_OF_RANGE
} TB6600TimingStatus;

/* Convert a requested step frequency to inclusive ARR and pulse-width CCR values. */
TB6600TimingStatus TB6600Timing_Calculate(uint32_t timer_tick_hz,
                                          uint32_t frequency_hz,
                                          uint32_t pulse_width_ticks,
                                          uint32_t counter_max,
                                          uint16_t *auto_reload,
                                          uint16_t *pulse_compare);

#endif /* TB6600_TIMING_H */

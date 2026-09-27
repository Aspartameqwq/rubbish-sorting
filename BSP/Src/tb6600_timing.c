#include "tb6600_timing.h"

#include <stddef.h>
#include <stdint.h>

TB6600TimingStatus TB6600Timing_Calculate(uint32_t timer_tick_hz,
                                          uint32_t frequency_hz,
                                          uint32_t pulse_width_ticks,
                                          uint32_t counter_max,
                                          uint16_t *auto_reload,
                                          uint16_t *pulse_compare)
{
    uint32_t period_counts;
    uint32_t arr;

    if ((auto_reload == NULL) || (pulse_compare == NULL) ||
        (timer_tick_hz == 0U) || (pulse_width_ticks == 0U) ||
        (counter_max > UINT16_MAX))
    {
        return TB6600_TIMING_STATUS_INVALID_ARGUMENT;
    }
    if (frequency_hz == 0U)
    {
        return TB6600_TIMING_STATUS_INVALID_ARGUMENT;
    }

    period_counts = timer_tick_hz / frequency_hz;
    if ((period_counts == 0U) || (period_counts > (counter_max + 1U)))
    {
        return TB6600_TIMING_STATUS_OUT_OF_RANGE;
    }

    arr = period_counts - 1U;
    if ((pulse_width_ticks >= arr) || (pulse_width_ticks > counter_max))
    {
        return TB6600_TIMING_STATUS_OUT_OF_RANGE;
    }

    *auto_reload = (uint16_t)arr;
    *pulse_compare = (uint16_t)pulse_width_ticks;
    return TB6600_TIMING_STATUS_OK;
}

#include "stepper_profile.h"

#include <stddef.h>

static uint32_t StepperProfile_IntegerSquareRoot(uint64_t value)
{
    uint64_t result = 0U;
    uint64_t bit = (uint64_t)1U << 62U;

    while (bit > value)
    {
        bit >>= 2U;
    }

    while (bit != 0U)
    {
        if (value >= (result + bit))
        {
            value -= result + bit;
            result = (result >> 1U) + bit;
        }
        else
        {
            result >>= 1U;
        }
        bit >>= 2U;
    }

    return (uint32_t)result;
}

static uint32_t StepperProfile_FrequencyAtDistance(const StepperProfileConfig *config,
                                                   uint32_t distance_steps)
{
    const uint64_t start_frequency_squared =
        (uint64_t)config->start_frequency_hz * config->start_frequency_hz;
    const uint64_t maximum_frequency_squared =
        (uint64_t)config->maximum_frequency_hz * config->maximum_frequency_hz;
    const uint64_t maximum_squared_delta = maximum_frequency_squared - start_frequency_squared;
    const uint64_t twice_acceleration =
        (uint64_t)config->acceleration_steps_per_second_squared * 2U;
    uint64_t squared_frequency;
    uint64_t steps_to_maximum;

    if (maximum_squared_delta == 0U)
    {
        return config->maximum_frequency_hz;
    }

    steps_to_maximum = maximum_squared_delta / twice_acceleration;
    if ((maximum_squared_delta % twice_acceleration) != 0U)
    {
        steps_to_maximum++;
    }

    if (distance_steps >= steps_to_maximum)
    {
        squared_frequency = maximum_frequency_squared;
    }
    else
    {
        /* This product is bounded by maximum_squared_delta, so it cannot overflow. */
        squared_frequency = start_frequency_squared +
                            ((uint64_t)distance_steps * twice_acceleration);
    }

    return StepperProfile_IntegerSquareRoot(squared_frequency);
}

StepperProfileStatus StepperProfile_Init(StepperProfile *profile,
                                         const StepperProfileConfig *config)
{
    uint32_t peak_distance;

    if ((profile == NULL) || (config == NULL))
    {
        return STEPPER_PROFILE_STATUS_INVALID_ARGUMENT;
    }

    profile->initialized = false;
    if ((config->total_steps == 0U) ||
        (config->start_frequency_hz == 0U) ||
        (config->maximum_frequency_hz < config->start_frequency_hz) ||
        (config->acceleration_steps_per_second_squared == 0U))
    {
        return STEPPER_PROFILE_STATUS_INVALID_ARGUMENT;
    }

    profile->config = *config;
    peak_distance = (config->total_steps - 1U) / 2U;
    profile->peak_frequency_hz =
        StepperProfile_FrequencyAtDistance(config, peak_distance);
    profile->initialized = true;
    return STEPPER_PROFILE_STATUS_OK;
}

StepperProfileStatus StepperProfile_GetFrequencyAtStep(const StepperProfile *profile,
                                                       uint32_t step_index,
                                                       uint32_t *frequency_hz)
{
    uint32_t distance_from_end;
    uint32_t distance;

    if ((profile == NULL) || (frequency_hz == NULL) || !profile->initialized)
    {
        return STEPPER_PROFILE_STATUS_INVALID_ARGUMENT;
    }
    if (step_index >= profile->config.total_steps)
    {
        return STEPPER_PROFILE_STATUS_OUT_OF_RANGE;
    }

    distance_from_end = profile->config.total_steps - 1U - step_index;
    distance = (step_index < distance_from_end) ? step_index : distance_from_end;
    *frequency_hz = StepperProfile_FrequencyAtDistance(&profile->config, distance);
    return STEPPER_PROFILE_STATUS_OK;
}

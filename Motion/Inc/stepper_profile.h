#ifndef STEPPER_PROFILE_H
#define STEPPER_PROFILE_H

#include <stdbool.h>
#include <stdint.h>

typedef enum
{
    STEPPER_PROFILE_STATUS_OK = 0,
    STEPPER_PROFILE_STATUS_INVALID_ARGUMENT,
    STEPPER_PROFILE_STATUS_OUT_OF_RANGE
} StepperProfileStatus;

typedef struct
{
    uint32_t total_steps;
    uint32_t start_frequency_hz;
    uint32_t maximum_frequency_hz;
    uint32_t acceleration_steps_per_second_squared;
} StepperProfileConfig;

typedef struct
{
    StepperProfileConfig config;
    uint32_t peak_frequency_hz;
    bool initialized;
} StepperProfile;

/* Initializes an integer-only, symmetric trapezoidal/triangular profile. */
StepperProfileStatus StepperProfile_Init(StepperProfile *profile,
                                         const StepperProfileConfig *config);

/* Returns the requested frequency for a zero-based step index. */
StepperProfileStatus StepperProfile_GetFrequencyAtStep(const StepperProfile *profile,
                                                       uint32_t step_index,
                                                       uint32_t *frequency_hz);

#endif /* STEPPER_PROFILE_H */

#ifndef TB6600_H
#define TB6600_H

#include <stdbool.h>
#include <stdint.h>

typedef enum
{
    TB6600_STATUS_OK = 0,
    TB6600_STATUS_INVALID_ARGUMENT,
    TB6600_STATUS_NOT_INITIALIZED,
    TB6600_STATUS_CONFIGURATION_ERROR,
    TB6600_STATUS_BUSY,
    TB6600_STATUS_HAL_ERROR
} TB6600Status;

typedef enum
{
    TB6600_DIRECTION_FORWARD = 0,
    TB6600_DIRECTION_REVERSE
} TB6600Direction;

/* Call after CubeMX has initialized TIM3 and GPIO. */
TB6600Status TB6600_Init(void);
TB6600Status TB6600_Enable(void);
TB6600Status TB6600_Disable(void);
TB6600Status TB6600_SetDirection(TB6600Direction direction);
TB6600Status TB6600_SetStepFrequency(uint32_t frequency_hz);
/* Start a finite hardware PWM burst; completion counts active-pulse compare edges. */
TB6600Status TB6600_StartPulse(uint32_t pulse_count);
/* Stop after the next completed active pulse width. */
TB6600Status TB6600_RequestPulseStop(void);
/* Stop immediately; the currently active pulse may be truncated. */
TB6600Status TB6600_StopPulse(void);

bool TB6600_IsInitialized(void);
bool TB6600_IsEnabled(void);
bool TB6600_IsPulseRunning(void);
bool TB6600_HasPulseError(void);
uint32_t TB6600_GetStepFrequency(void);
/* Completed compare events for the current/most recent burst. */
uint32_t TB6600_GetCompletedPulseCount(void);
uint32_t TB6600_GetTickMs(void);

#endif /* TB6600_H */

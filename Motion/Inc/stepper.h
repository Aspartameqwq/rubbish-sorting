#ifndef STEPPER_H
#define STEPPER_H

#include <stdbool.h>
#include <stdint.h>

typedef enum
{
    STEPPER_STATUS_OK = 0,
    STEPPER_STATUS_INVALID_ARGUMENT,
    STEPPER_STATUS_NOT_INITIALIZED,
    STEPPER_STATUS_DISABLED,
    STEPPER_STATUS_BUSY,
    STEPPER_STATUS_DRIVER_ERROR,
    STEPPER_STATUS_FAULT
} StepperStatus;

typedef enum
{
    STEPPER_STATE_UNINITIALIZED = 0,
    STEPPER_STATE_DISABLED,
    STEPPER_STATE_IDLE,
    STEPPER_STATE_DIRECTION_SETUP,
    STEPPER_STATE_RUNNING,
    STEPPER_STATE_STOPPING,
    STEPPER_STATE_FAULT
} StepperState;

StepperStatus Stepper_Init(void);
StepperStatus Stepper_Enable(void);
StepperStatus Stepper_Disable(void);
/* Schedule a signed open-loop move at the requested steps per second. */
StepperStatus Stepper_MoveSteps(int32_t steps, uint32_t frequency_hz);
/* Graceful stop at the next completed pulse boundary. */
StepperStatus Stepper_Stop(void);
/* Immediate stop; an active pulse may be truncated. */
StepperStatus Stepper_EmergencyStop(void);
void Stepper_Process(void);

bool Stepper_IsBusy(void);
bool Stepper_IsEnabled(void);
StepperState Stepper_GetState(void);
/* Completed pulse count maintained by firmware; not measured shaft position. */
int32_t Stepper_GetCommandedPosition(void);
uint32_t Stepper_GetRemainingSteps(void);
uint32_t Stepper_GetStepFrequency(void);

#endif /* STEPPER_H */

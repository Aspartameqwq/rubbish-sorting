#include "stepper.h"

#include "project_config.h"
#include "tb6600.h"
#include "tb6600_timing.h"

#include <limits.h>
#include <stddef.h>

static bool s_initialized;
static bool s_disable_after_stop;
static StepperState s_state = STEPPER_STATE_UNINITIALIZED;
static TB6600Direction s_move_direction;
static uint32_t s_direction_setup_started_ms;
static uint32_t s_requested_steps;
static uint32_t s_remaining_steps;
static uint32_t s_observed_pulses;
static uint32_t s_step_frequency_hz;
static int32_t s_commanded_position;

static void Stepper_ResetMove(void)
{
    s_requested_steps = 0U;
    s_remaining_steps = 0U;
    s_observed_pulses = 0U;
    s_step_frequency_hz = 0U;
    s_disable_after_stop = false;
}

static StepperStatus Stepper_ApplyPulseProgress(void)
{
    uint32_t completed_pulses = TB6600_GetCompletedPulseCount();
    uint32_t delta;
    int64_t position;

    if (completed_pulses < s_observed_pulses)
    {
        return STEPPER_STATUS_FAULT;
    }
    if (completed_pulses > s_requested_steps)
    {
        return STEPPER_STATUS_FAULT;
    }

    delta = completed_pulses - s_observed_pulses;
    if (delta != 0U)
    {
        const int64_t signed_delta = (s_move_direction == TB6600_DIRECTION_FORWARD)
                                         ? (int64_t)delta
                                         : -(int64_t)delta;
        position = (int64_t)s_commanded_position + signed_delta;
        if ((position < INT32_MIN) || (position > INT32_MAX))
        {
            return STEPPER_STATUS_FAULT;
        }
        s_commanded_position = (int32_t)position;
        s_observed_pulses = completed_pulses;
        s_remaining_steps = (completed_pulses >= s_requested_steps)
                                ? 0U
                                : s_requested_steps - completed_pulses;
    }
    return STEPPER_STATUS_OK;
}

static StepperStatus Stepper_Fault(void)
{
    TB6600Status stop_status = TB6600_StopPulse();
    s_state = STEPPER_STATE_FAULT;
    return (stop_status == TB6600_STATUS_OK) ? STEPPER_STATUS_DRIVER_ERROR
                                             : STEPPER_STATUS_FAULT;
}

StepperStatus Stepper_Init(void)
{
    if (!TB6600_IsInitialized())
    {
        s_state = STEPPER_STATE_UNINITIALIZED;
        return STEPPER_STATUS_DRIVER_ERROR;
    }

    if (TB6600_Disable() != TB6600_STATUS_OK)
    {
        s_state = STEPPER_STATE_FAULT;
        return STEPPER_STATUS_DRIVER_ERROR;
    }

    Stepper_ResetMove();
    s_move_direction = TB6600_DIRECTION_FORWARD;
    s_direction_setup_started_ms = 0U;
    s_commanded_position = 0;
    s_initialized = true;
    s_state = STEPPER_STATE_DISABLED;
    return STEPPER_STATUS_OK;
}

StepperStatus Stepper_Enable(void)
{
    TB6600Status driver_status;

    if (!s_initialized)
    {
        return STEPPER_STATUS_NOT_INITIALIZED;
    }
    if (s_state == STEPPER_STATE_FAULT)
    {
        return STEPPER_STATUS_FAULT;
    }
    if (s_state != STEPPER_STATE_DISABLED)
    {
        return STEPPER_STATUS_OK;
    }

    driver_status = TB6600_Enable();
    if (driver_status != TB6600_STATUS_OK)
    {
        return Stepper_Fault();
    }
    s_state = STEPPER_STATE_IDLE;
    return STEPPER_STATUS_OK;
}

StepperStatus Stepper_Disable(void)
{
    TB6600Status driver_status;

    if (!s_initialized)
    {
        return STEPPER_STATUS_NOT_INITIALIZED;
    }
    if (s_state == STEPPER_STATE_FAULT)
    {
        return STEPPER_STATUS_FAULT;
    }
    if (s_state == STEPPER_STATE_DISABLED)
    {
        return STEPPER_STATUS_OK;
    }

    if (s_state == STEPPER_STATE_RUNNING)
    {
        driver_status = TB6600_RequestPulseStop();
        if (driver_status != TB6600_STATUS_OK)
        {
            return Stepper_Fault();
        }
        s_disable_after_stop = true;
        s_state = STEPPER_STATE_STOPPING;
        return STEPPER_STATUS_OK;
    }

    if (s_state == STEPPER_STATE_STOPPING)
    {
        s_disable_after_stop = true;
        return STEPPER_STATUS_OK;
    }

    Stepper_ResetMove();
    driver_status = TB6600_Disable();
    if (driver_status != TB6600_STATUS_OK)
    {
        return Stepper_Fault();
    }
    s_state = STEPPER_STATE_DISABLED;
    return STEPPER_STATUS_OK;
}

StepperStatus Stepper_MoveSteps(int32_t steps, uint32_t frequency_hz)
{
    uint32_t magnitude;
    int64_t target_position;
    TB6600TimingStatus timing_status;
    uint16_t unused_arr;
    uint16_t unused_ccr;

    if (!s_initialized)
    {
        return STEPPER_STATUS_NOT_INITIALIZED;
    }
    if (s_state == STEPPER_STATE_FAULT)
    {
        return STEPPER_STATUS_FAULT;
    }
    if ((s_state == STEPPER_STATE_DIRECTION_SETUP) ||
        (s_state == STEPPER_STATE_RUNNING) ||
        (s_state == STEPPER_STATE_STOPPING))
    {
        return STEPPER_STATUS_BUSY;
    }

    timing_status = TB6600Timing_Calculate(TB6600_TIMER_TICK_HZ,
                                           frequency_hz,
                                           TB6600_PULSE_HIGH_US,
                                           UINT16_MAX,
                                           &unused_arr,
                                           &unused_ccr);
    if ((frequency_hz < TB6600_STEP_FREQ_MIN_HZ) ||
        (frequency_hz > TB6600_STEP_FREQ_MAX_HZ) ||
        (timing_status != TB6600_TIMING_STATUS_OK))
    {
        return STEPPER_STATUS_INVALID_ARGUMENT;
    }
    if (steps == 0)
    {
        return STEPPER_STATUS_OK;
    }
    if (s_state != STEPPER_STATE_IDLE)
    {
        return STEPPER_STATUS_DISABLED;
    }

    magnitude = (steps < 0)
                    ? (uint32_t)(-(int64_t)steps)
                    : (uint32_t)steps;
    target_position = (int64_t)s_commanded_position + (int64_t)steps;
    if ((target_position < INT32_MIN) || (target_position > INT32_MAX))
    {
        return STEPPER_STATUS_INVALID_ARGUMENT;
    }

    if ((TB6600_SetStepFrequency(frequency_hz) != TB6600_STATUS_OK) ||
        (TB6600_SetDirection((steps > 0) ? TB6600_DIRECTION_FORWARD
                                         : TB6600_DIRECTION_REVERSE) != TB6600_STATUS_OK))
    {
        return Stepper_Fault();
    }

    s_move_direction = (steps > 0) ? TB6600_DIRECTION_FORWARD : TB6600_DIRECTION_REVERSE;
    s_requested_steps = magnitude;
    s_remaining_steps = magnitude;
    s_observed_pulses = 0U;
    s_step_frequency_hz = frequency_hz;
    s_direction_setup_started_ms = TB6600_GetTickMs();
    s_disable_after_stop = false;
    s_state = STEPPER_STATE_DIRECTION_SETUP;
    return STEPPER_STATUS_OK;
}

StepperStatus Stepper_Stop(void)
{
    TB6600Status driver_status;

    if (!s_initialized)
    {
        return STEPPER_STATUS_NOT_INITIALIZED;
    }
    if (s_state == STEPPER_STATE_FAULT)
    {
        return STEPPER_STATUS_FAULT;
    }
    if (s_state == STEPPER_STATE_DIRECTION_SETUP)
    {
        Stepper_ResetMove();
        s_state = STEPPER_STATE_IDLE;
        return STEPPER_STATUS_OK;
    }
    if (s_state == STEPPER_STATE_RUNNING)
    {
        driver_status = TB6600_RequestPulseStop();
        if (driver_status != TB6600_STATUS_OK)
        {
            return Stepper_Fault();
        }
        s_state = STEPPER_STATE_STOPPING;
    }
    return STEPPER_STATUS_OK;
}

StepperStatus Stepper_EmergencyStop(void)
{
    TB6600Status driver_status;

    if (!s_initialized)
    {
        return STEPPER_STATUS_NOT_INITIALIZED;
    }
    if (s_state == STEPPER_STATE_FAULT)
    {
        driver_status = TB6600_StopPulse();
        return (driver_status == TB6600_STATUS_OK) ? STEPPER_STATUS_FAULT
                                                   : STEPPER_STATUS_DRIVER_ERROR;
    }

    driver_status = TB6600_StopPulse();
    if (driver_status != TB6600_STATUS_OK)
    {
        return Stepper_Fault();
    }
    if (((s_state == STEPPER_STATE_RUNNING) || (s_state == STEPPER_STATE_STOPPING)) &&
        (Stepper_ApplyPulseProgress() != STEPPER_STATUS_OK))
    {
        return Stepper_Fault();
    }

    Stepper_ResetMove();
    s_state = TB6600_IsEnabled() ? STEPPER_STATE_IDLE : STEPPER_STATE_DISABLED;
    return STEPPER_STATUS_OK;
}

void Stepper_Process(void)
{
    if (!s_initialized || (s_state == STEPPER_STATE_FAULT))
    {
        return;
    }

    if (s_state == STEPPER_STATE_DIRECTION_SETUP)
    {
        if ((uint32_t)(TB6600_GetTickMs() - s_direction_setup_started_ms) >=
            STEPPER_DIRECTION_SETUP_MS)
        {
            if (TB6600_StartPulse(s_requested_steps) != TB6600_STATUS_OK)
            {
                (void)Stepper_Fault();
                return;
            }
            s_state = STEPPER_STATE_RUNNING;
        }
        return;
    }

    if ((s_state != STEPPER_STATE_RUNNING) && (s_state != STEPPER_STATE_STOPPING))
    {
        return;
    }

    if (Stepper_ApplyPulseProgress() != STEPPER_STATUS_OK)
    {
        (void)Stepper_Fault();
        return;
    }
    if (TB6600_HasPulseError())
    {
        (void)Stepper_Fault();
        return;
    }
    if (TB6600_IsPulseRunning())
    {
        return;
    }

    if (s_disable_after_stop)
    {
        if (TB6600_Disable() != TB6600_STATUS_OK)
        {
            (void)Stepper_Fault();
            return;
        }
        s_state = STEPPER_STATE_DISABLED;
    }
    else
    {
        s_state = STEPPER_STATE_IDLE;
    }
    Stepper_ResetMove();
}

bool Stepper_IsBusy(void)
{
    return (s_state == STEPPER_STATE_DIRECTION_SETUP) ||
           (s_state == STEPPER_STATE_RUNNING) ||
           (s_state == STEPPER_STATE_STOPPING);
}

bool Stepper_IsEnabled(void)
{
    return (s_state != STEPPER_STATE_UNINITIALIZED) &&
           (s_state != STEPPER_STATE_DISABLED) &&
           (s_state != STEPPER_STATE_FAULT) && TB6600_IsEnabled();
}

StepperState Stepper_GetState(void)
{
    return s_state;
}

int32_t Stepper_GetCommandedPosition(void)
{
    return s_commanded_position;
}

uint32_t Stepper_GetRemainingSteps(void)
{
    return s_remaining_steps;
}

uint32_t Stepper_GetStepFrequency(void)
{
    return s_step_frequency_hz;
}

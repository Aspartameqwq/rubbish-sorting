#include "yaw_axis.h"

#include "control_debug_config.h"
#include "project_config.h"
#include "stepper.h"

#include <limits.h>

#if (YAW_SOFT_LIMIT_VALID != 0U) && (YAW_SOFT_LIMIT_VALID != 1U)
#error "YAW_SOFT_LIMIT_VALID must be 0 or 1"
#endif
#if (YAW_SOFT_LIMIT_VALID != 0U) && (YAW_SOFT_MIN_MDEG >= YAW_SOFT_MAX_MDEG)
#error "Yaw software limits must have MIN < MAX"
#endif
#if (YAW_PULSES_PER_REV == 0U) || (ANGLE_MDEG_PER_REV <= 0L) || \
    ((ANGLE_MDEG_PER_REV % YAW_PULSES_PER_REV) != 0U)
#error "Yaw pulses per revolution must divide the millidegree revolution exactly"
#endif

#define YAW_MDEG_PER_PULSE (ANGLE_MDEG_PER_REV / YAW_PULSES_PER_REV)

static bool s_initialized;
static YawReferenceState s_reference_state = YAW_REFERENCE_INVALID;
static int32_t s_zero_offset_pulses;
static int32_t s_target_mdeg = INT32_MIN;
static int32_t s_quantized_target_mdeg = INT32_MIN;
static uint32_t s_limit_reject_count;
static YawAxisStatus s_last_status = YAW_AXIS_STATUS_NOT_INITIALIZED;

static void YawAxis_RecordStatus(YawAxisStatus status)
{
    s_last_status = status;
    if ((status == YAW_AXIS_STATUS_LIMIT) && (s_limit_reject_count < UINT32_MAX))
    {
        s_limit_reject_count++;
    }
}

static YawAxisStatus YawAxis_MapStepperStatus(StepperStatus status)
{
    switch (status)
    {
        case STEPPER_STATUS_OK:
            return YAW_AXIS_STATUS_OK;
        case STEPPER_STATUS_INVALID_ARGUMENT:
            return YAW_AXIS_STATUS_INVALID_ARGUMENT;
        case STEPPER_STATUS_NOT_INITIALIZED:
            return YAW_AXIS_STATUS_NOT_INITIALIZED;
        case STEPPER_STATUS_DISABLED:
            return YAW_AXIS_STATUS_DISABLED;
        case STEPPER_STATUS_BUSY:
            return YAW_AXIS_STATUS_BUSY;
        case STEPPER_STATUS_DRIVER_ERROR:
        case STEPPER_STATUS_FAULT:
        default:
            return YAW_AXIS_STATUS_DRIVER_ERROR;
    }
}

static bool YawAxis_ConvertTarget(int32_t target_mdeg,
                                  int32_t *relative_pulses,
                                  int32_t *quantized_mdeg)
{
    const int64_t numerator = (int64_t)target_mdeg * YAW_PULSES_PER_REV;
    const int64_t denominator = ANGLE_MDEG_PER_REV;
    int64_t rounded_pulses;
    int64_t applied_mdeg;

    if ((relative_pulses == NULL) || (quantized_mdeg == NULL))
    {
        return false;
    }

    if (numerator >= 0)
    {
        rounded_pulses = (numerator + (denominator / 2)) / denominator;
    }
    else
    {
        rounded_pulses = -(((-numerator) + (denominator / 2)) / denominator);
    }

    if ((rounded_pulses < INT32_MIN) || (rounded_pulses > INT32_MAX))
    {
        return false;
    }
    applied_mdeg = rounded_pulses * YAW_MDEG_PER_PULSE;
    if ((applied_mdeg < INT32_MIN) || (applied_mdeg > INT32_MAX))
    {
        return false;
    }

    *relative_pulses = (int32_t)rounded_pulses;
    *quantized_mdeg = (int32_t)applied_mdeg;
    return true;
}

YawAxisStatus YawAxis_Init(void)
{
    s_initialized = false;
    s_reference_state = YAW_REFERENCE_INVALID;
    s_zero_offset_pulses = 0;
    s_target_mdeg = INT32_MIN;
    s_quantized_target_mdeg = INT32_MIN;
    s_limit_reject_count = 0U;

    if (Stepper_GetState() == STEPPER_STATE_UNINITIALIZED)
    {
        s_last_status = YAW_AXIS_STATUS_NOT_INITIALIZED;
        return s_last_status;
    }

    s_initialized = true;
    s_last_status = YAW_AXIS_STATUS_NOT_REFERENCED;
    return YAW_AXIS_STATUS_OK;
}

YawAxisStatus YawAxis_Enable(void)
{
    YawAxisStatus status;

    if (!s_initialized)
    {
        YawAxis_RecordStatus(YAW_AXIS_STATUS_NOT_INITIALIZED);
        return YAW_AXIS_STATUS_NOT_INITIALIZED;
    }
    status = YawAxis_MapStepperStatus(Stepper_Enable());
    YawAxis_RecordStatus(status);
    return status;
}

YawAxisStatus YawAxis_Disable(void)
{
    YawAxisStatus status;

    if (!s_initialized)
    {
        YawAxis_RecordStatus(YAW_AXIS_STATUS_NOT_INITIALIZED);
        return YAW_AXIS_STATUS_NOT_INITIALIZED;
    }
    status = YawAxis_MapStepperStatus(Stepper_Disable());
    YawAxis_RecordStatus(status);
    return status;
}

YawAxisStatus YawAxis_ValidateTargetMilliDeg(int32_t target_mdeg,
                                              uint32_t pulse_frequency_hz)
{
    int32_t relative_pulses;
    int32_t quantized_mdeg;
    int64_t absolute_target;
    int64_t delta;

    if (!s_initialized)
    {
        return YAW_AXIS_STATUS_NOT_INITIALIZED;
    }
    if (s_reference_state == YAW_REFERENCE_INVALID)
    {
        return YAW_AXIS_STATUS_NOT_REFERENCED;
    }
    if ((pulse_frequency_hz < TB6600_STEP_FREQ_MIN_HZ) ||
        (pulse_frequency_hz > TB6600_STEP_FREQ_MAX_HZ))
    {
        return YAW_AXIS_STATUS_INVALID_ARGUMENT;
    }
    if (Stepper_IsBusy())
    {
        return YAW_AXIS_STATUS_BUSY;
    }
    if (!Stepper_IsEnabled())
    {
        return YAW_AXIS_STATUS_DISABLED;
    }
    if (!YawAxis_ConvertTarget(target_mdeg, &relative_pulses, &quantized_mdeg))
    {
        return YAW_AXIS_STATUS_INVALID_ARGUMENT;
    }
#if (YAW_SOFT_LIMIT_VALID == 1U)
    if ((target_mdeg < YAW_SOFT_MIN_MDEG) || (target_mdeg > YAW_SOFT_MAX_MDEG) ||
        (quantized_mdeg < YAW_SOFT_MIN_MDEG) || (quantized_mdeg > YAW_SOFT_MAX_MDEG))
    {
        return YAW_AXIS_STATUS_LIMIT;
    }
#endif

    absolute_target = (int64_t)s_zero_offset_pulses + relative_pulses;
    if ((absolute_target < INT32_MIN) || (absolute_target > INT32_MAX))
    {
        return YAW_AXIS_STATUS_INVALID_ARGUMENT;
    }
    delta = absolute_target - Stepper_GetCommandedPosition();
    if ((delta < INT32_MIN) || (delta > INT32_MAX))
    {
        return YAW_AXIS_STATUS_INVALID_ARGUMENT;
    }
    return YAW_AXIS_STATUS_OK;
}

YawAxisStatus YawAxis_SetTargetMilliDeg(int32_t target_mdeg,
                                        uint32_t pulse_frequency_hz)
{
    YawAxisStatus status = YawAxis_ValidateTargetMilliDeg(target_mdeg, pulse_frequency_hz);
    int32_t relative_pulses;
    int32_t quantized_mdeg;
    int64_t absolute_target;
    int32_t delta;

    if (status != YAW_AXIS_STATUS_OK)
    {
        YawAxis_RecordStatus(status);
        return status;
    }
    if (!YawAxis_ConvertTarget(target_mdeg, &relative_pulses, &quantized_mdeg))
    {
        YawAxis_RecordStatus(YAW_AXIS_STATUS_INVALID_ARGUMENT);
        return YAW_AXIS_STATUS_INVALID_ARGUMENT;
    }

    absolute_target = (int64_t)s_zero_offset_pulses + relative_pulses;
    delta = (int32_t)(absolute_target - Stepper_GetCommandedPosition());
    status = YawAxis_MapStepperStatus(Stepper_MoveSteps(delta, pulse_frequency_hz));
    if (status == YAW_AXIS_STATUS_OK)
    {
        s_target_mdeg = target_mdeg;
        s_quantized_target_mdeg = quantized_mdeg;
    }
    YawAxis_RecordStatus(status);
    return status;
}

YawAxisStatus YawAxis_SetCurrentPositionAsZero(void)
{
    if (!s_initialized)
    {
        YawAxis_RecordStatus(YAW_AXIS_STATUS_NOT_INITIALIZED);
        return YAW_AXIS_STATUS_NOT_INITIALIZED;
    }
    if (Stepper_IsBusy())
    {
        YawAxis_RecordStatus(YAW_AXIS_STATUS_BUSY);
        return YAW_AXIS_STATUS_BUSY;
    }
    if (Stepper_GetState() == STEPPER_STATE_FAULT)
    {
        YawAxis_RecordStatus(YAW_AXIS_STATUS_DRIVER_ERROR);
        return YAW_AXIS_STATUS_DRIVER_ERROR;
    }

    s_zero_offset_pulses = Stepper_GetCommandedPosition();
    s_reference_state = YAW_REFERENCE_MANUAL;
    s_target_mdeg = 0;
    s_quantized_target_mdeg = 0;
    YawAxis_RecordStatus(YAW_AXIS_STATUS_OK);
    return YAW_AXIS_STATUS_OK;
}

YawAxisStatus YawAxis_Stop(void)
{
    YawAxisStatus status;

    if (!s_initialized)
    {
        YawAxis_RecordStatus(YAW_AXIS_STATUS_NOT_INITIALIZED);
        return YAW_AXIS_STATUS_NOT_INITIALIZED;
    }
    status = YawAxis_MapStepperStatus(Stepper_Stop());
    YawAxis_RecordStatus(status);
    return status;
}

void YawAxis_Process(void)
{
    if (s_initialized)
    {
        Stepper_Process();
    }
}

int32_t YawAxis_GetTargetMilliDeg(void)
{
    return s_target_mdeg;
}

int32_t YawAxis_GetQuantizedTargetMilliDeg(void)
{
    return s_quantized_target_mdeg;
}

int32_t YawAxis_GetCommandedMilliDeg(void)
{
    int64_t relative_pulses;
    int64_t angle_mdeg;

    if (!s_initialized || (s_reference_state == YAW_REFERENCE_INVALID))
    {
        return INT32_MIN;
    }
    relative_pulses = (int64_t)Stepper_GetCommandedPosition() - s_zero_offset_pulses;
    angle_mdeg = relative_pulses * YAW_MDEG_PER_PULSE;
    if ((angle_mdeg < INT32_MIN) || (angle_mdeg > INT32_MAX))
    {
        return INT32_MIN;
    }
    return (int32_t)angle_mdeg;
}

int32_t YawAxis_GetMeasuredMilliDeg(void)
{
    return INT32_MIN;
}

int32_t YawAxis_GetCommandedPositionPulses(void)
{
    return Stepper_GetCommandedPosition();
}

int32_t YawAxis_GetZeroOffsetPulses(void)
{
    return s_zero_offset_pulses;
}

uint32_t YawAxis_GetRemainingPulses(void)
{
    return Stepper_GetRemainingSteps();
}

uint32_t YawAxis_GetPulseFrequencyHz(void)
{
    return Stepper_GetStepFrequency();
}

uint32_t YawAxis_GetStepperState(void)
{
    return (uint32_t)Stepper_GetState();
}

bool YawAxis_IsMeasurementValid(void)
{
    return false;
}

bool YawAxis_IsBusy(void)
{
    return s_initialized && Stepper_IsBusy();
}

bool YawAxis_IsEnabled(void)
{
    return s_initialized && Stepper_IsEnabled();
}

YawReferenceState YawAxis_GetReferenceState(void)
{
    return s_reference_state;
}

bool YawAxis_IsSoftLimitEnabled(void)
{
    return YAW_SOFT_LIMIT_VALID != 0U;
}

int32_t YawAxis_GetSoftLimitMinMilliDeg(void)
{
    return YAW_SOFT_MIN_MDEG;
}

int32_t YawAxis_GetSoftLimitMaxMilliDeg(void)
{
    return YAW_SOFT_MAX_MDEG;
}

uint32_t YawAxis_GetLimitRejectCount(void)
{
    return s_limit_reject_count;
}

YawAxisStatus YawAxis_GetLastStatus(void)
{
    return s_last_status;
}

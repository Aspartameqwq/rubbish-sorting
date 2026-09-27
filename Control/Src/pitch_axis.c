#include "pitch_axis.h"

#include "project_config.h"
#include "servo.h"

#include <limits.h>

#if (PITCH_SOFT_LIMIT_VALID != 0U) && (PITCH_SOFT_LIMIT_VALID != 1U)
#error "PITCH_SOFT_LIMIT_VALID must be 0 or 1"
#endif
#if (PITCH_SOFT_LIMIT_VALID != 0U) && (PITCH_SOFT_MIN_MDEG >= PITCH_SOFT_MAX_MDEG)
#error "Pitch software limits must have MIN < MAX"
#endif
#if (SERVO_CALIBRATION_VALID != 0U) && (SERVO_CALIBRATION_VALID != 1U)
#error "SERVO_CALIBRATION_VALID must be 0 or 1"
#endif

static bool s_initialized;
static bool s_raw_pulse_mode;
static int32_t s_target_mdeg;
static uint32_t s_limit_reject_count;
static PitchAxisStatus s_last_status = PITCH_AXIS_STATUS_NOT_INITIALIZED;

static void PitchAxis_RecordStatus(PitchAxisStatus status)
{
    s_last_status = status;
    if ((status == PITCH_AXIS_STATUS_LIMIT) && (s_limit_reject_count < UINT32_MAX))
    {
        s_limit_reject_count++;
    }
}

PitchAxisStatus PitchAxis_Init(void)
{
    s_initialized = false;
    s_raw_pulse_mode = false;
    s_target_mdeg = INT32_MIN;
    s_limit_reject_count = 0U;

    if (!Servo_IsInitialized())
    {
        s_last_status = PITCH_AXIS_STATUS_NOT_INITIALIZED;
        return s_last_status;
    }

    s_target_mdeg = Servo_GetTargetAngleMilliDeg();
    s_initialized = true;
    s_last_status = PITCH_AXIS_STATUS_OK;
    return s_last_status;
}

PitchAxisStatus PitchAxis_ValidateTargetMilliDeg(int32_t target_mdeg)
{
    if (!s_initialized)
    {
        return PITCH_AXIS_STATUS_NOT_INITIALIZED;
    }
    if ((target_mdeg < ((int32_t)SERVO_MIN_ANGLE_DEG * 1000)) ||
        (target_mdeg > ((int32_t)SERVO_MAX_ANGLE_DEG * 1000)))
    {
        return PITCH_AXIS_STATUS_INVALID_ARGUMENT;
    }
#if (PITCH_SOFT_LIMIT_VALID == 1U)
    if ((target_mdeg < PITCH_SOFT_MIN_MDEG) || (target_mdeg > PITCH_SOFT_MAX_MDEG))
    {
        return PITCH_AXIS_STATUS_LIMIT;
    }
#endif
    return PITCH_AXIS_STATUS_OK;
}

PitchAxisStatus PitchAxis_SetTargetMilliDeg(int32_t target_mdeg)
{
    PitchAxisStatus status = PitchAxis_ValidateTargetMilliDeg(target_mdeg);
    ServoStatus servo_status;

    if (status != PITCH_AXIS_STATUS_OK)
    {
        PitchAxis_RecordStatus(status);
        return status;
    }

    servo_status = Servo_SetAngleMilliDeg(target_mdeg);
    if (servo_status != SERVO_STATUS_OK)
    {
        status = (servo_status == SERVO_STATUS_INVALID_ARGUMENT)
                     ? PITCH_AXIS_STATUS_INVALID_ARGUMENT
                     : PITCH_AXIS_STATUS_DRIVER_ERROR;
        PitchAxis_RecordStatus(status);
        return status;
    }

    s_target_mdeg = target_mdeg;
    s_raw_pulse_mode = false;
    PitchAxis_RecordStatus(PITCH_AXIS_STATUS_OK);
    return PITCH_AXIS_STATUS_OK;
}

PitchAxisStatus PitchAxis_SetRawPulseUs(uint32_t pulse_us)
{
    ServoStatus servo_status;

    if (!s_initialized)
    {
        PitchAxis_RecordStatus(PITCH_AXIS_STATUS_NOT_INITIALIZED);
        return PITCH_AXIS_STATUS_NOT_INITIALIZED;
    }
    if (pulse_us > UINT16_MAX)
    {
        PitchAxis_RecordStatus(PITCH_AXIS_STATUS_INVALID_ARGUMENT);
        return PITCH_AXIS_STATUS_INVALID_ARGUMENT;
    }

    servo_status = Servo_SetPulseUs((uint16_t)pulse_us);
    if (servo_status != SERVO_STATUS_OK)
    {
        PitchAxisStatus status = (servo_status == SERVO_STATUS_INVALID_ARGUMENT)
                                     ? PITCH_AXIS_STATUS_INVALID_ARGUMENT
                                     : PITCH_AXIS_STATUS_DRIVER_ERROR;
        PitchAxis_RecordStatus(status);
        return status;
    }

    s_target_mdeg = INT32_MIN;
    s_raw_pulse_mode = true;
    PitchAxis_RecordStatus(PITCH_AXIS_STATUS_OK);
    return PITCH_AXIS_STATUS_OK;
}

int32_t PitchAxis_GetTargetMilliDeg(void)
{
    return (s_initialized && Servo_IsAngleValid()) ? s_target_mdeg : INT32_MIN;
}

int32_t PitchAxis_GetCommandedMilliDeg(void)
{
    return (s_initialized && Servo_IsAngleValid())
               ? Servo_GetCommandedAngleMilliDeg()
               : INT32_MIN;
}

int32_t PitchAxis_GetMeasuredMilliDeg(void)
{
    return INT32_MIN;
}

bool PitchAxis_IsMeasurementValid(void)
{
    return false;
}

bool PitchAxis_IsCommandedAngleValid(void)
{
    return s_initialized && Servo_IsAngleValid();
}

bool PitchAxis_IsRawPulseMode(void)
{
    return s_raw_pulse_mode;
}

bool PitchAxis_IsServoEnabled(void)
{
    return s_initialized && Servo_IsEnabled();
}

bool PitchAxis_IsCalibrationValid(void)
{
    return SERVO_CALIBRATION_VALID != 0U;
}

bool PitchAxis_IsSoftLimitEnabled(void)
{
    return PITCH_SOFT_LIMIT_VALID != 0U;
}

int32_t PitchAxis_GetSoftLimitMinMilliDeg(void)
{
    return PITCH_SOFT_MIN_MDEG;
}

int32_t PitchAxis_GetSoftLimitMaxMilliDeg(void)
{
    return PITCH_SOFT_MAX_MDEG;
}

uint32_t PitchAxis_GetLimitRejectCount(void)
{
    return s_limit_reject_count;
}

PitchAxisStatus PitchAxis_GetLastStatus(void)
{
    return s_last_status;
}

uint32_t PitchAxis_GetPulseUs(void)
{
    return Servo_IsInitialized() ? Servo_GetPulseUs() : 0U;
}

#include "pitch_axis.h"

#include "control_debug_config.h"
#include "servo.h"

#include <limits.h>
#include <stddef.h>

static bool s_initialized;
static bool s_raw_pulse_mode;
static bool s_moving;
static bool s_clock_started;
static int32_t s_target_mdeg;
static int32_t s_commanded_mdeg;
static int32_t s_trajectory_start_mdeg;
static int32_t s_servo_target_mdeg;
static uint32_t s_response_time_ms;
static uint32_t s_active_response_time_ms;
static uint32_t s_trajectory_start_ms;
static uint32_t s_trajectory_elapsed_ms;
static uint32_t s_current_time_ms;
static uint32_t s_last_update_ms;
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

static bool PitchAxis_ToServoAngle(int32_t pitch_mdeg, int32_t *servo_mdeg)
{
    const int64_t mapped = (int64_t)PITCH_LEVEL_SERVO_MDEG +
                           ((int64_t)PITCH_SERVO_DIRECTION_SIGN * pitch_mdeg);

    if ((servo_mdeg == NULL) || (mapped < SERVO_MIN_ANGLE_MDEG) ||
        (mapped > SERVO_MAX_ANGLE_MDEG))
    {
        return false;
    }

    *servo_mdeg = (int32_t)mapped;
    return true;
}

#if (RAW_BENCH_COMMANDS_ENABLE == 1)
static bool PitchAxis_FromServoAngle(int32_t servo_mdeg, int32_t *pitch_mdeg)
{
    const int64_t mapped = ((int64_t)servo_mdeg - PITCH_LEVEL_SERVO_MDEG) *
                           PITCH_SERVO_DIRECTION_SIGN;

    if ((pitch_mdeg == NULL) || (mapped < INT32_MIN) || (mapped > INT32_MAX))
    {
        return false;
    }

    *pitch_mdeg = (int32_t)mapped;
    return true;
}
#endif

PitchAxisStatus PitchAxis_Init(void)
{
    s_initialized = false;
    s_raw_pulse_mode = false;
    s_moving = false;
    s_clock_started = false;
    s_target_mdeg = 0;
    s_commanded_mdeg = 0;
    s_trajectory_start_mdeg = 0;
    s_servo_target_mdeg = PITCH_LEVEL_SERVO_MDEG;
    s_response_time_ms = PITCH_RESPONSE_TIME_DEFAULT_MS;
    s_active_response_time_ms = PITCH_RESPONSE_TIME_DEFAULT_MS;
    s_trajectory_start_ms = 0U;
    s_trajectory_elapsed_ms = 0U;
    s_current_time_ms = 0U;
    s_last_update_ms = 0U;
    s_limit_reject_count = 0U;

    if (!Servo_IsInitialized())
    {
        s_last_status = PITCH_AXIS_STATUS_NOT_INITIALIZED;
        return s_last_status;
    }

    /* Establish the mechanically observed horizontal reference at startup. */
    if (Servo_SetAngleMilliDeg(PITCH_LEVEL_SERVO_MDEG) != SERVO_STATUS_OK)
    {
        s_last_status = PITCH_AXIS_STATUS_DRIVER_ERROR;
        return s_last_status;
    }

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
    if (!Servo_IsEnabled())
    {
        return PITCH_AXIS_STATUS_DISABLED;
    }
    if ((target_mdeg < PITCH_SOFT_MIN_MDEG) || (target_mdeg > PITCH_SOFT_MAX_MDEG))
    {
        return PITCH_AXIS_STATUS_LIMIT;
    }

    return PITCH_AXIS_STATUS_OK;
}

PitchAxisStatus PitchAxis_SetTargetMilliDeg(int32_t target_mdeg)
{
    PitchAxisStatus status = PitchAxis_ValidateTargetMilliDeg(target_mdeg);

    if (status != PITCH_AXIS_STATUS_OK)
    {
        PitchAxis_RecordStatus(status);
        return status;
    }

    s_target_mdeg = target_mdeg;
    s_trajectory_start_mdeg = s_commanded_mdeg;
    s_trajectory_start_ms = s_current_time_ms;
    s_trajectory_elapsed_ms = 0U;
    s_active_response_time_ms = s_response_time_ms;
    s_raw_pulse_mode = false;
    s_moving = (s_target_mdeg != s_trajectory_start_mdeg);
    PitchAxis_RecordStatus(PITCH_AXIS_STATUS_OK);
    return PITCH_AXIS_STATUS_OK;
}

PitchAxisStatus PitchAxis_SetRawPulseUs(uint32_t pulse_us)
{
#if (RAW_BENCH_COMMANDS_ENABLE == 1)
    int32_t servo_angle_mdeg;
    int32_t pitch_mdeg;
    PitchAxisStatus status;
    ServoStatus servo_status;

    if (!s_initialized)
    {
        PitchAxis_RecordStatus(PITCH_AXIS_STATUS_NOT_INITIALIZED);
        return PITCH_AXIS_STATUS_NOT_INITIALIZED;
    }
    if (!Servo_IsEnabled())
    {
        PitchAxis_RecordStatus(PITCH_AXIS_STATUS_DISABLED);
        return PITCH_AXIS_STATUS_DISABLED;
    }
    if (pulse_us > UINT16_MAX ||
        (Servo_ConvertPulseUsToAngleMilliDeg((uint16_t)pulse_us,
                                             &servo_angle_mdeg) != SERVO_STATUS_OK))
    {
        PitchAxis_RecordStatus(PITCH_AXIS_STATUS_INVALID_ARGUMENT);
        return PITCH_AXIS_STATUS_INVALID_ARGUMENT;
    }
    if (!PitchAxis_FromServoAngle(servo_angle_mdeg, &pitch_mdeg))
    {
        PitchAxis_RecordStatus(PITCH_AXIS_STATUS_INVALID_ARGUMENT);
        return PITCH_AXIS_STATUS_INVALID_ARGUMENT;
    }
    status = PitchAxis_ValidateTargetMilliDeg(pitch_mdeg);
    if (status != PITCH_AXIS_STATUS_OK)
    {
        PitchAxis_RecordStatus(status);
        return status;
    }

    servo_status = Servo_SetPulseUs((uint16_t)pulse_us);
    if (servo_status != SERVO_STATUS_OK)
    {
        status = (servo_status == SERVO_STATUS_INVALID_ARGUMENT)
                     ? PITCH_AXIS_STATUS_INVALID_ARGUMENT
                     : PITCH_AXIS_STATUS_DRIVER_ERROR;
        PitchAxis_RecordStatus(status);
        return status;
    }

    s_target_mdeg = INT32_MIN;
    s_commanded_mdeg = pitch_mdeg;
    s_trajectory_start_mdeg = pitch_mdeg;
    s_servo_target_mdeg = servo_angle_mdeg;
    s_trajectory_elapsed_ms = 0U;
    s_raw_pulse_mode = true;
    s_moving = false;
    PitchAxis_RecordStatus(PITCH_AXIS_STATUS_OK);
    return PITCH_AXIS_STATUS_OK;
#else
    (void)pulse_us;
    PitchAxis_RecordStatus(PITCH_AXIS_STATUS_DISABLED);
    return PITCH_AXIS_STATUS_DISABLED;
#endif
}

PitchAxisStatus PitchAxis_SetRawServoAngleMilliDeg(int32_t servo_angle_mdeg)
{
#if (RAW_BENCH_COMMANDS_ENABLE == 1)
    int32_t pitch_mdeg;
    int32_t quantized_servo_mdeg;
    uint16_t pulse_us;
    PitchAxisStatus status;
    ServoStatus servo_status;

    if (!s_initialized)
    {
        PitchAxis_RecordStatus(PITCH_AXIS_STATUS_NOT_INITIALIZED);
        return PITCH_AXIS_STATUS_NOT_INITIALIZED;
    }
    if (!Servo_IsEnabled())
    {
        PitchAxis_RecordStatus(PITCH_AXIS_STATUS_DISABLED);
        return PITCH_AXIS_STATUS_DISABLED;
    }
    if (Servo_ConvertAngleMilliDegToPulseUs(servo_angle_mdeg, &pulse_us) != SERVO_STATUS_OK ||
        Servo_ConvertPulseUsToAngleMilliDeg(pulse_us, &quantized_servo_mdeg) != SERVO_STATUS_OK ||
        !PitchAxis_FromServoAngle(quantized_servo_mdeg, &pitch_mdeg))
    {
        PitchAxis_RecordStatus(PITCH_AXIS_STATUS_INVALID_ARGUMENT);
        return PITCH_AXIS_STATUS_INVALID_ARGUMENT;
    }
    status = PitchAxis_ValidateTargetMilliDeg(pitch_mdeg);
    if (status != PITCH_AXIS_STATUS_OK)
    {
        PitchAxis_RecordStatus(status);
        return status;
    }

    servo_status = Servo_SetAngleMilliDeg(servo_angle_mdeg);
    if (servo_status != SERVO_STATUS_OK)
    {
        status = (servo_status == SERVO_STATUS_INVALID_ARGUMENT)
                     ? PITCH_AXIS_STATUS_INVALID_ARGUMENT
                     : PITCH_AXIS_STATUS_DRIVER_ERROR;
        PitchAxis_RecordStatus(status);
        return status;
    }

    s_target_mdeg = INT32_MIN;
    s_commanded_mdeg = pitch_mdeg;
    s_trajectory_start_mdeg = pitch_mdeg;
    s_servo_target_mdeg = servo_angle_mdeg;
    s_trajectory_elapsed_ms = 0U;
    s_raw_pulse_mode = true;
    s_moving = false;
    PitchAxis_RecordStatus(PITCH_AXIS_STATUS_OK);
    return PITCH_AXIS_STATUS_OK;
#else
    (void)servo_angle_mdeg;
    PitchAxis_RecordStatus(PITCH_AXIS_STATUS_DISABLED);
    return PITCH_AXIS_STATUS_DISABLED;
#endif
}

PitchAxisStatus PitchAxis_SetResponseTimeMs(uint32_t response_time_ms)
{
    if (!s_initialized)
    {
        PitchAxis_RecordStatus(PITCH_AXIS_STATUS_NOT_INITIALIZED);
        return PITCH_AXIS_STATUS_NOT_INITIALIZED;
    }
    if ((response_time_ms < PITCH_RESPONSE_TIME_MIN_MS) ||
        (response_time_ms > PITCH_RESPONSE_TIME_MAX_MS))
    {
        PitchAxis_RecordStatus(PITCH_AXIS_STATUS_INVALID_ARGUMENT);
        return PITCH_AXIS_STATUS_INVALID_ARGUMENT;
    }

    s_response_time_ms = response_time_ms;
    PitchAxis_RecordStatus(PITCH_AXIS_STATUS_OK);
    return PITCH_AXIS_STATUS_OK;
}

void PitchAxis_Process(uint32_t now_ms)
{
    int32_t next_commanded_mdeg;
    int32_t servo_angle_mdeg;
    uint32_t elapsed_ms;

    if (!s_initialized)
    {
        return;
    }

    s_current_time_ms = now_ms;
    if (!s_clock_started)
    {
        s_last_update_ms = now_ms;
        s_clock_started = true;
        return;
    }
    if ((uint32_t)(now_ms - s_last_update_ms) < PITCH_UPDATE_PERIOD_MS)
    {
        return;
    }
    s_last_update_ms = now_ms;

    if (!s_moving || s_raw_pulse_mode)
    {
        return;
    }

    elapsed_ms = (uint32_t)(now_ms - s_trajectory_start_ms);
    if (elapsed_ms >= s_active_response_time_ms)
    {
        next_commanded_mdeg = s_target_mdeg;
        s_trajectory_elapsed_ms = s_active_response_time_ms;
    }
    else
    {
        const int64_t delta_mdeg = (int64_t)s_target_mdeg - s_trajectory_start_mdeg;
        const int64_t progressed_mdeg = delta_mdeg * elapsed_ms / s_active_response_time_ms;
        next_commanded_mdeg = (int32_t)((int64_t)s_trajectory_start_mdeg + progressed_mdeg);
        s_trajectory_elapsed_ms = elapsed_ms;
    }

    if (!PitchAxis_ToServoAngle(next_commanded_mdeg, &servo_angle_mdeg))
    {
        s_moving = false;
        PitchAxis_RecordStatus(PITCH_AXIS_STATUS_LIMIT);
        return;
    }

    if (Servo_SetAngleMilliDeg(servo_angle_mdeg) != SERVO_STATUS_OK)
    {
        PitchAxis_RecordStatus(PITCH_AXIS_STATUS_DRIVER_ERROR);
        return;
    }

    s_commanded_mdeg = next_commanded_mdeg;
    s_servo_target_mdeg = servo_angle_mdeg;
    s_raw_pulse_mode = false;
    if (elapsed_ms >= s_active_response_time_ms)
    {
        s_moving = false;
    }
    PitchAxis_RecordStatus(PITCH_AXIS_STATUS_OK);
}

int32_t PitchAxis_GetTargetMilliDeg(void)
{
    return s_initialized ? s_target_mdeg : INT32_MIN;
}

int32_t PitchAxis_GetCommandedMilliDeg(void)
{
    return s_initialized ? s_commanded_mdeg : INT32_MIN;
}

int32_t PitchAxis_GetServoTargetMilliDeg(void)
{
    return s_initialized ? s_servo_target_mdeg : INT32_MIN;
}

int32_t PitchAxis_GetMeasuredMilliDeg(void)
{
    return INT32_MIN;
}

uint32_t PitchAxis_GetResponseTimeMs(void)
{
    return s_response_time_ms;
}

uint32_t PitchAxis_GetActiveResponseTimeMs(void)
{
    return s_active_response_time_ms;
}

uint32_t PitchAxis_GetTrajectoryElapsedMs(void)
{
    if (!s_initialized)
    {
        return 0U;
    }
    if (s_moving)
    {
        uint32_t elapsed_ms = (uint32_t)(s_current_time_ms - s_trajectory_start_ms);
        return (elapsed_ms > s_active_response_time_ms) ? s_active_response_time_ms
                                                       : elapsed_ms;
    }
    return s_trajectory_elapsed_ms;
}

bool PitchAxis_IsMoving(void)
{
    return s_initialized && s_moving;
}

bool PitchAxis_IsMeasurementValid(void)
{
    return false;
}

bool PitchAxis_IsCommandedAngleValid(void)
{
    return s_initialized;
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
    return true;
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

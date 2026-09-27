#include "debug_state.h"

#include "pitch_axis.h"
#include "sort_task.h"
#include "yaw_axis.h"

#include <limits.h>
#include <stdbool.h>
#include <stddef.h>

volatile ControlDebugBlock g_control_debug;

static uint32_t s_heartbeat;
static uint32_t s_last_snapshot_ms;
static uint32_t s_last_debug_command;
static int32_t s_last_debug_result;
static uint32_t s_tuning_reject_count;
static bool s_snapshot_started;

#if (DEBUG_CONTROL_ENABLE == 1)
static int32_t Debug_MapPitchStatus(PitchAxisStatus status)
{
    switch (status)
    {
        case PITCH_AXIS_STATUS_OK:
            return DEBUG_RESULT_OK;
        case PITCH_AXIS_STATUS_INVALID_ARGUMENT:
            return DEBUG_RESULT_INVALID_ARGUMENT;
        case PITCH_AXIS_STATUS_NOT_INITIALIZED:
            return DEBUG_RESULT_NOT_INITIALIZED;
        case PITCH_AXIS_STATUS_LIMIT:
            return DEBUG_RESULT_LIMIT;
        case PITCH_AXIS_STATUS_DISABLED:
            return DEBUG_RESULT_DISABLED;
        case PITCH_AXIS_STATUS_DRIVER_ERROR:
        default:
            return DEBUG_RESULT_DRIVER_ERROR;
    }
}

static int32_t Debug_MapYawStatus(YawAxisStatus status)
{
    switch (status)
    {
        case YAW_AXIS_STATUS_OK:
            return DEBUG_RESULT_OK;
        case YAW_AXIS_STATUS_INVALID_ARGUMENT:
            return DEBUG_RESULT_INVALID_ARGUMENT;
        case YAW_AXIS_STATUS_NOT_INITIALIZED:
            return DEBUG_RESULT_NOT_INITIALIZED;
        case YAW_AXIS_STATUS_NOT_REFERENCED:
            return DEBUG_RESULT_NOT_REFERENCED;
        case YAW_AXIS_STATUS_LIMIT:
            return DEBUG_RESULT_LIMIT;
        case YAW_AXIS_STATUS_BUSY:
            return DEBUG_RESULT_BUSY;
        case YAW_AXIS_STATUS_DISABLED:
            return DEBUG_RESULT_AXIS_DISABLED;
        case YAW_AXIS_STATUS_DRIVER_ERROR:
            return DEBUG_RESULT_DRIVER_ERROR;
        case YAW_AXIS_STATUS_INVALID_STATE:
            return DEBUG_RESULT_INVALID_STATE;
        default:
            return DEBUG_RESULT_DRIVER_ERROR;
    }
}

static int32_t Debug_ExecuteCommand(uint32_t command,
                                    int32_t pitch_target_mdeg,
                                    int32_t yaw_target_mdeg,
                                    uint32_t yaw_frequency_hz,
                                    uint32_t pitch_pulse_us,
                                    uint32_t pitch_response_time_ms)
{
    if (SortTask_IsBusy() && (command != DEBUG_CMD_YAW_STOP))
    {
        return DEBUG_RESULT_BUSY;
    }

    switch (command)
    {
        case DEBUG_CMD_SET_PITCH_MDEG:
            return Debug_MapPitchStatus(PitchAxis_SetTargetMilliDeg(pitch_target_mdeg));

        case DEBUG_CMD_SET_PITCH_PULSE_US:
            return Debug_MapPitchStatus(PitchAxis_SetRawPulseUs(pitch_pulse_us));

        case DEBUG_CMD_SET_YAW_MDEG:
            return Debug_MapYawStatus(YawAxis_SetTargetMilliDeg(yaw_target_mdeg,
                                                                 yaw_frequency_hz));

        case DEBUG_CMD_SET_BOTH_MDEG:
        {
            PitchAxisStatus pitch_validation =
                PitchAxis_ValidateTargetMilliDeg(pitch_target_mdeg);
            YawAxisStatus yaw_validation =
                YawAxis_ValidateTargetMilliDeg(yaw_target_mdeg, yaw_frequency_hz);
            PitchAxisStatus pitch_status;
            YawAxisStatus yaw_status;

            if (pitch_validation != PITCH_AXIS_STATUS_OK)
            {
                (void)PitchAxis_SetTargetMilliDeg(pitch_target_mdeg);
                return Debug_MapPitchStatus(pitch_validation);
            }
            if (yaw_validation != YAW_AXIS_STATUS_OK)
            {
                (void)YawAxis_SetTargetMilliDeg(yaw_target_mdeg, yaw_frequency_hz);
                return Debug_MapYawStatus(yaw_validation);
            }

            pitch_status = PitchAxis_SetTargetMilliDeg(pitch_target_mdeg);
            if (pitch_status != PITCH_AXIS_STATUS_OK)
            {
                return Debug_MapPitchStatus(pitch_status);
            }
            yaw_status = YawAxis_SetTargetMilliDeg(yaw_target_mdeg, yaw_frequency_hz);
            return (yaw_status == YAW_AXIS_STATUS_OK)
                       ? DEBUG_RESULT_OK
                       : DEBUG_RESULT_PARTIAL;
        }

        case DEBUG_CMD_SET_YAW_ZERO:
            return Debug_MapYawStatus(YawAxis_SetCurrentPositionAsZero());

        case DEBUG_CMD_YAW_ENABLE:
            return Debug_MapYawStatus(YawAxis_Enable());

        case DEBUG_CMD_YAW_DISABLE:
            return Debug_MapYawStatus(YawAxis_Disable());

        case DEBUG_CMD_YAW_STOP:
            return Debug_MapYawStatus(YawAxis_Stop());

        case DEBUG_CMD_SET_PITCH_RESPONSE_MS:
        {
            PitchAxisStatus status = PitchAxis_SetResponseTimeMs(pitch_response_time_ms);
            if (status == PITCH_AXIS_STATUS_OK)
            {
                g_control_debug.tuning.pitch_response_time_ms = pitch_response_time_ms;
            }
            return Debug_MapPitchStatus(status);
        }

        case DEBUG_CMD_NONE:
        default:
            return DEBUG_RESULT_UNKNOWN_COMMAND;
    }
}
#endif

#if (DEBUG_CONTROL_ENABLE != 1)
static int32_t Debug_ExecuteCommissioningCommand(uint32_t command)
{
    YawAxisStatus status;

    if (SortTask_IsBusy() && (command != DEBUG_CMD_YAW_STOP))
    {
        return DEBUG_RESULT_BUSY;
    }

    switch (command)
    {
        case DEBUG_CMD_SET_YAW_ZERO:
            status = YawAxis_SetCurrentPositionAsZero();
            break;
        case DEBUG_CMD_YAW_ENABLE:
            status = YawAxis_Enable();
            break;
        case DEBUG_CMD_YAW_DISABLE:
            status = YawAxis_Disable();
            break;
        case DEBUG_CMD_YAW_STOP:
            status = YawAxis_Stop();
            break;
        default:
            return DEBUG_RESULT_DISABLED;
    }

    switch (status)
    {
        case YAW_AXIS_STATUS_OK:
            return DEBUG_RESULT_OK;
        case YAW_AXIS_STATUS_NOT_INITIALIZED:
            return DEBUG_RESULT_NOT_INITIALIZED;
        case YAW_AXIS_STATUS_NOT_REFERENCED:
            return DEBUG_RESULT_NOT_REFERENCED;
        case YAW_AXIS_STATUS_LIMIT:
            return DEBUG_RESULT_LIMIT;
        case YAW_AXIS_STATUS_BUSY:
            return DEBUG_RESULT_BUSY;
        case YAW_AXIS_STATUS_DISABLED:
            return DEBUG_RESULT_AXIS_DISABLED;
        case YAW_AXIS_STATUS_INVALID_ARGUMENT:
            return DEBUG_RESULT_INVALID_ARGUMENT;
        case YAW_AXIS_STATUS_INVALID_STATE:
            return DEBUG_RESULT_INVALID_STATE;
        case YAW_AXIS_STATUS_DRIVER_ERROR:
        default:
            return DEBUG_RESULT_DRIVER_ERROR;
    }
}
#endif

static void Debug_ApplyTuning(void)
{
#if (DEBUG_CONTROL_ENABLE == 1)
    const uint32_t requested_ms = g_control_debug.tuning.pitch_response_time_ms;
    const uint32_t active_setting_ms = PitchAxis_GetResponseTimeMs();

    if (requested_ms != active_setting_ms)
    {
        if (PitchAxis_SetResponseTimeMs(requested_ms) == PITCH_AXIS_STATUS_OK)
        {
            return;
        }

        g_control_debug.tuning.pitch_response_time_ms = active_setting_ms;
        if (s_tuning_reject_count < UINT32_MAX)
        {
            s_tuning_reject_count++;
        }
    }
#else
    g_control_debug.tuning.pitch_response_time_ms = PitchAxis_GetResponseTimeMs();
#endif
}

static DebugState Debug_CaptureSnapshot(uint32_t now_ms, uint32_t app_health_flags)
{
    DebugState snapshot = {0};

    snapshot.version = DEBUG_STATE_VERSION;
    snapshot.heartbeat = ++s_heartbeat;
    snapshot.tick_ms = now_ms;
    snapshot.app_health_flags = app_health_flags;
    snapshot.last_debug_command = s_last_debug_command;
    snapshot.last_debug_result = s_last_debug_result;

    snapshot.pitch.target_mdeg = PitchAxis_GetTargetMilliDeg();
    snapshot.pitch.commanded_mdeg = PitchAxis_GetCommandedMilliDeg();
    snapshot.pitch.servo_target_mdeg = PitchAxis_GetServoTargetMilliDeg();
    snapshot.pitch.servo_pulse_us = PitchAxis_GetPulseUs();
    snapshot.pitch.measured_mdeg = PitchAxis_GetMeasuredMilliDeg();
    snapshot.pitch.measurement_valid = PitchAxis_IsMeasurementValid() ? 1U : 0U;
    snapshot.pitch.moving = PitchAxis_IsMoving() ? 1U : 0U;
    snapshot.pitch.response_time_ms = PitchAxis_GetResponseTimeMs();
    snapshot.pitch.active_response_time_ms = PitchAxis_GetActiveResponseTimeMs();
    snapshot.pitch.trajectory_elapsed_ms = PitchAxis_GetTrajectoryElapsedMs();
    snapshot.pitch.soft_limit_min_mdeg = PitchAxis_GetSoftLimitMinMilliDeg();
    snapshot.pitch.soft_limit_max_mdeg = PitchAxis_GetSoftLimitMaxMilliDeg();
    snapshot.pitch.limit_reject_count = PitchAxis_GetLimitRejectCount();
    snapshot.pitch.tuning_reject_count = s_tuning_reject_count;
    snapshot.pitch.servo_enabled = PitchAxis_IsServoEnabled() ? 1U : 0U;
    snapshot.pitch.calibration_valid = PitchAxis_IsCalibrationValid() ? 1U : 0U;
    snapshot.pitch.raw_pulse_mode = PitchAxis_IsRawPulseMode() ? 1U : 0U;
    snapshot.pitch.status = (int32_t)PitchAxis_GetLastStatus();

    snapshot.yaw.target_mdeg = YawAxis_GetTargetMilliDeg();
    snapshot.yaw.quantized_target_mdeg = YawAxis_GetQuantizedTargetMilliDeg();
    snapshot.yaw.commanded_mdeg = YawAxis_GetCommandedMilliDeg();
    snapshot.yaw.measured_mdeg = YawAxis_GetMeasuredMilliDeg();
    snapshot.yaw.measurement_valid = YawAxis_IsMeasurementValid() ? 1U : 0U;
    snapshot.yaw.commanded_position_pulses = YawAxis_GetCommandedPositionPulses();
    snapshot.yaw.zero_offset_pulses = YawAxis_GetZeroOffsetPulses();
    snapshot.yaw.remaining_pulses = YawAxis_GetRemainingPulses();
    snapshot.yaw.pulse_frequency_hz = YawAxis_GetPulseFrequencyHz();
    snapshot.yaw.stepper_state = YawAxis_GetStepperState();
    snapshot.yaw.enabled = YawAxis_IsEnabled() ? 1U : 0U;
    snapshot.yaw.reference_state = (uint32_t)YawAxis_GetReferenceState();
    snapshot.yaw.soft_limit_min_mdeg = YawAxis_GetSoftLimitMinMilliDeg();
    snapshot.yaw.soft_limit_max_mdeg = YawAxis_GetSoftLimitMaxMilliDeg();
    snapshot.yaw.soft_limit_enabled = YawAxis_IsSoftLimitEnabled() ? 1U : 0U;
    snapshot.yaw.limit_reject_count = YawAxis_GetLimitRejectCount();
    snapshot.yaw.status = (int32_t)YawAxis_GetLastStatus();
    snapshot.yaw.cable_limit_min_mdeg = YAW_CABLE_LIMIT_MIN_MDEG;
    snapshot.yaw.cable_limit_max_mdeg = YAW_CABLE_LIMIT_MAX_MDEG;
    snapshot.yaw.cable_limit_min_pulses = YawAxis_GetCableLimitMinPulses();
    snapshot.yaw.cable_limit_max_pulses = YawAxis_GetCableLimitMaxPulses();
    snapshot.yaw.cable_margin_to_min_mdeg = YawAxis_GetCableMarginToMinMilliDeg();
    snapshot.yaw.cable_margin_to_max_mdeg = YawAxis_GetCableMarginToMaxMilliDeg();
    snapshot.yaw.cable_remaining_negative_pulses =
        YawAxis_GetCableRemainingNegativePulses();
    snapshot.yaw.cable_remaining_positive_pulses =
        YawAxis_GetCableRemainingPositivePulses();
    snapshot.yaw.cable_limit_reject_count = YawAxis_GetLimitRejectCount();
    snapshot.yaw.axis_pulses_per_rev = YAW_AXIS_PULSES_PER_REV;
    snapshot.yaw.mdeg_per_pulse = ANGLE_MDEG_PER_REV / YAW_AXIS_PULSES_PER_REV;
    snapshot.yaw.axis_scale_verified = YAW_AXIS_SCALE_VERIFIED;
    snapshot.yaw.frequency_min_hz = YAW_STEP_FREQ_MIN_HZ;
    snapshot.yaw.frequency_max_hz = YAW_STEP_FREQ_MAX_HZ;
    snapshot.yaw.cable_margin_valid =
        (YawAxis_GetReferenceState() != YAW_REFERENCE_INVALID) ? 1U : 0U;

    return snapshot;
}

static void Debug_StoreSnapshot(const DebugState *snapshot)
{
    volatile unsigned char *destination = (volatile unsigned char *)&g_control_debug.state;
    const unsigned char *source = (const unsigned char *)snapshot;
    uint32_t sequence = g_control_debug.state.snapshot_seq;
    size_t index;

    if ((sequence & 1U) != 0U)
    {
        sequence++;
    }
    g_control_debug.state.snapshot_seq = sequence + 1U;

    for (index = sizeof(snapshot->snapshot_seq); index < sizeof(*snapshot); index++)
    {
        destination[index] = source[index];
    }

    g_control_debug.state.snapshot_seq = sequence + 2U;
}

void Debug_Init(void)
{
    const ControlDebugBlock empty_block = {0};

    g_control_debug = empty_block;
    g_control_debug.state.version = DEBUG_STATE_VERSION;
    g_control_debug.state.pitch.target_mdeg = DEBUG_ANGLE_INVALID_MDEG;
    g_control_debug.state.pitch.commanded_mdeg = DEBUG_ANGLE_INVALID_MDEG;
    g_control_debug.state.pitch.servo_target_mdeg = DEBUG_ANGLE_INVALID_MDEG;
    g_control_debug.state.pitch.measured_mdeg = DEBUG_ANGLE_INVALID_MDEG;
    g_control_debug.state.yaw.target_mdeg = DEBUG_ANGLE_INVALID_MDEG;
    g_control_debug.state.yaw.quantized_target_mdeg = DEBUG_ANGLE_INVALID_MDEG;
    g_control_debug.state.yaw.commanded_mdeg = DEBUG_ANGLE_INVALID_MDEG;
    g_control_debug.state.yaw.measured_mdeg = DEBUG_ANGLE_INVALID_MDEG;
    g_control_debug.state.yaw.soft_limit_min_mdeg = YAW_CABLE_LIMIT_MIN_MDEG;
    g_control_debug.state.yaw.soft_limit_max_mdeg = YAW_CABLE_LIMIT_MAX_MDEG;
    g_control_debug.state.yaw.soft_limit_enabled = 1U;
    g_control_debug.state.yaw.cable_limit_min_mdeg = YAW_CABLE_LIMIT_MIN_MDEG;
    g_control_debug.state.yaw.cable_limit_max_mdeg = YAW_CABLE_LIMIT_MAX_MDEG;
    g_control_debug.state.yaw.cable_limit_min_pulses = YAW_CABLE_LIMIT_MIN_PULSES;
    g_control_debug.state.yaw.cable_limit_max_pulses = YAW_CABLE_LIMIT_MAX_PULSES;
    g_control_debug.state.yaw.cable_margin_to_min_mdeg = DEBUG_ANGLE_INVALID_MDEG;
    g_control_debug.state.yaw.cable_margin_to_max_mdeg = DEBUG_ANGLE_INVALID_MDEG;
    g_control_debug.state.yaw.axis_pulses_per_rev = YAW_AXIS_PULSES_PER_REV;
    g_control_debug.state.yaw.mdeg_per_pulse =
        ANGLE_MDEG_PER_REV / YAW_AXIS_PULSES_PER_REV;
    g_control_debug.state.yaw.axis_scale_verified = YAW_AXIS_SCALE_VERIFIED;
    g_control_debug.state.yaw.frequency_min_hz = YAW_STEP_FREQ_MIN_HZ;
    g_control_debug.state.yaw.frequency_max_hz = YAW_STEP_FREQ_MAX_HZ;
    g_control_debug.state.yaw.cable_margin_valid = 0U;
    g_control_debug.state.last_debug_result = DEBUG_RESULT_OK;
    g_control_debug.command.result = DEBUG_RESULT_OK;
    g_control_debug.tuning.pitch_response_time_ms = PITCH_RESPONSE_TIME_DEFAULT_MS;

    s_heartbeat = 0U;
    s_last_snapshot_ms = 0U;
    s_last_debug_command = DEBUG_CMD_NONE;
    s_last_debug_result = DEBUG_RESULT_OK;
    s_tuning_reject_count = 0U;
    s_snapshot_started = false;
}

void Debug_Process(uint32_t now_ms, uint32_t app_health_flags)
{
    const uint32_t request_seq = g_control_debug.command.request_seq;
    const uint32_t applied_seq = g_control_debug.command.applied_seq;

    Debug_ApplyTuning();

    if (request_seq != applied_seq)
    {
        const uint32_t command = g_control_debug.command.command;
        int32_t result;

#if (DEBUG_CONTROL_ENABLE == 1)
        const int32_t pitch_target_mdeg = g_control_debug.command.pitch_target_mdeg;
        const int32_t yaw_target_mdeg = g_control_debug.command.yaw_target_mdeg;
        const uint32_t yaw_frequency_hz = g_control_debug.command.yaw_frequency_hz;
        const uint32_t pitch_pulse_us = g_control_debug.command.pitch_pulse_us;
        const uint32_t pitch_response_time_ms = g_control_debug.command.pitch_response_time_ms;

        result = Debug_ExecuteCommand(command,
                                      pitch_target_mdeg,
                                      yaw_target_mdeg,
                                      yaw_frequency_hz,
                                      pitch_pulse_us,
                                      pitch_response_time_ms);
#else
        result = Debug_ExecuteCommissioningCommand(command);
#endif

        g_control_debug.command.result = result;
        s_last_debug_command = command;
        s_last_debug_result = result;
        g_control_debug.command.command = DEBUG_CMD_NONE;
        g_control_debug.command.applied_seq = request_seq;
    }

    if (!s_snapshot_started ||
        ((uint32_t)(now_ms - s_last_snapshot_ms) >= DEBUG_SNAPSHOT_PERIOD_MS))
    {
        const DebugState snapshot = Debug_CaptureSnapshot(now_ms, app_health_flags);
        Debug_StoreSnapshot(&snapshot);
        s_last_snapshot_ms = now_ms;
        s_snapshot_started = true;
    }
}

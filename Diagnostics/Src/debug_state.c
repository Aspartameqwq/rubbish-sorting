#include "debug_state.h"

#include "pitch_axis.h"
#include "yaw_axis.h"

#include <limits.h>
#include <stdbool.h>

#if (DEBUG_CONTROL_ENABLE != 0) && (DEBUG_CONTROL_ENABLE != 1)
#error "DEBUG_CONTROL_ENABLE must be 0 or 1"
#endif

volatile DebugState g_debug_state;
volatile DebugCommand g_debug_command;

static uint32_t s_heartbeat;
static uint32_t s_last_snapshot_ms;
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
        default:
            return DEBUG_RESULT_DRIVER_ERROR;
    }
}

static int32_t Debug_ExecuteCommand(uint32_t command,
                                    int32_t pitch_target_mdeg,
                                    int32_t yaw_target_mdeg,
                                    uint32_t yaw_frequency_hz,
                                    uint32_t pitch_pulse_us)
{
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

        case DEBUG_CMD_NONE:
        default:
            return DEBUG_RESULT_UNKNOWN_COMMAND;
    }
}
#endif

static DebugState Debug_CaptureSnapshot(uint32_t now_ms, uint32_t app_health_flags)
{
    DebugState snapshot = {0};

    snapshot.version = DEBUG_STATE_VERSION;
    snapshot.heartbeat = ++s_heartbeat;
    snapshot.tick_ms = now_ms;
    snapshot.app_health_flags = app_health_flags;

    snapshot.pitch.target_mdeg = PitchAxis_GetTargetMilliDeg();
    snapshot.pitch.commanded_mdeg = PitchAxis_GetCommandedMilliDeg();
    snapshot.pitch.measured_mdeg = PitchAxis_GetMeasuredMilliDeg();
    snapshot.pitch.measurement_valid = PitchAxis_IsMeasurementValid() ? 1U : 0U;
    snapshot.pitch.pulse_us = PitchAxis_GetPulseUs();
    snapshot.pitch.servo_enabled = PitchAxis_IsServoEnabled() ? 1U : 0U;
    snapshot.pitch.calibration_valid = PitchAxis_IsCalibrationValid() ? 1U : 0U;
    snapshot.pitch.raw_pulse_mode = PitchAxis_IsRawPulseMode() ? 1U : 0U;
    snapshot.pitch.soft_limit_min_mdeg = PitchAxis_GetSoftLimitMinMilliDeg();
    snapshot.pitch.soft_limit_max_mdeg = PitchAxis_GetSoftLimitMaxMilliDeg();
    snapshot.pitch.soft_limit_enabled = PitchAxis_IsSoftLimitEnabled() ? 1U : 0U;
    snapshot.pitch.limit_reject_count = PitchAxis_GetLimitRejectCount();
    snapshot.pitch.control_error_mdeg = DEBUG_ANGLE_INVALID_MDEG;
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
    snapshot.yaw.control_error_mdeg = DEBUG_ANGLE_INVALID_MDEG;
    snapshot.yaw.status = (int32_t)YawAxis_GetLastStatus();

    return snapshot;
}

void Debug_Init(void)
{
    const DebugState empty_state = {0};
    const DebugCommand empty_command = {0};

    g_debug_state = empty_state;
    g_debug_command = empty_command;
    g_debug_state.version = DEBUG_STATE_VERSION;
    g_debug_state.pitch.measured_mdeg = DEBUG_ANGLE_INVALID_MDEG;
    g_debug_state.pitch.control_error_mdeg = DEBUG_ANGLE_INVALID_MDEG;
    g_debug_state.yaw.target_mdeg = DEBUG_ANGLE_INVALID_MDEG;
    g_debug_state.yaw.quantized_target_mdeg = DEBUG_ANGLE_INVALID_MDEG;
    g_debug_state.yaw.commanded_mdeg = DEBUG_ANGLE_INVALID_MDEG;
    g_debug_state.yaw.measured_mdeg = DEBUG_ANGLE_INVALID_MDEG;
    g_debug_state.yaw.control_error_mdeg = DEBUG_ANGLE_INVALID_MDEG;
    g_debug_command.result = DEBUG_RESULT_OK;
    s_heartbeat = 0U;
    s_last_snapshot_ms = 0U;
    s_snapshot_started = false;
}

void Debug_Process(uint32_t now_ms, uint32_t app_health_flags)
{
    const uint32_t request_seq = g_debug_command.request_seq;
    const uint32_t applied_seq = g_debug_command.applied_seq;

    if (request_seq != applied_seq)
    {
#if (DEBUG_CONTROL_ENABLE == 1)
        const uint32_t command = g_debug_command.command;
        const int32_t pitch_target_mdeg = g_debug_command.pitch_target_mdeg;
        const int32_t yaw_target_mdeg = g_debug_command.yaw_target_mdeg;
        const uint32_t yaw_frequency_hz = g_debug_command.yaw_frequency_hz;
        const uint32_t pitch_pulse_us = g_debug_command.pitch_pulse_us;
        g_debug_command.result = Debug_ExecuteCommand(command,
                                                       pitch_target_mdeg,
                                                       yaw_target_mdeg,
                                                       yaw_frequency_hz,
                                                       pitch_pulse_us);
#else
        g_debug_command.result = DEBUG_RESULT_DISABLED;
#endif
        g_debug_command.applied_seq = request_seq;
    }

    if (!s_snapshot_started ||
        ((uint32_t)(now_ms - s_last_snapshot_ms) >= DEBUG_SNAPSHOT_PERIOD_MS))
    {
        const DebugState snapshot = Debug_CaptureSnapshot(now_ms, app_health_flags);
        g_debug_state = snapshot;
        s_last_snapshot_ms = now_ms;
        s_snapshot_started = true;
    }
}

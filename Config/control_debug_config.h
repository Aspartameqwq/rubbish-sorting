#ifndef CONTROL_DEBUG_CONFIG_H
#define CONTROL_DEBUG_CONFIG_H

#include <stdint.h>
#include "project_config.h"

/* =============================
 * Servo calibration and Pitch reference
 * ============================= */

#ifndef PITCH_LEVEL_SERVO_MDEG
#define PITCH_LEVEL_SERVO_MDEG              148000L
#endif

#define SERVO_MIN_ANGLE_DEG                 0L
#define SERVO_MAX_ANGLE_DEG                 270L
#define SERVO_MIN_ANGLE_MDEG                (SERVO_MIN_ANGLE_DEG * 1000L)
#define SERVO_MAX_ANGLE_MDEG                (SERVO_MAX_ANGLE_DEG * 1000L)
#define SERVO_CENTER_ANGLE_MDEG             135000L
/* Display / legacy integer-degree value only; never use in calibration math. */
#define SERVO_CENTER_ANGLE_DEG              (SERVO_CENTER_ANGLE_MDEG / 1000L)

/* User-supplied 270-degree Servo reference: 0.5/1.5/2.5 ms at 0/135/270 deg. */
#define SERVO_MIN_PULSE_US                  500U
#define SERVO_CENTER_PULSE_US               1500U
#define SERVO_MAX_PULSE_US                  2500U
#ifndef SERVO_CALIBRATION_VALID
#define SERVO_CALIBRATION_VALID             0U
#endif

/* Bench-observed horizontal at Servo 148 deg; 1:1 Pitch-to-Servo angle relation. */
#define PITCH_SERVO_DIRECTION_SIGN          (+1)

/* Hard logical Pitch limits; there is intentionally no disable switch. */
#define PITCH_SOFT_MIN_MDEG                 (-45000L)
#define PITCH_SOFT_MAX_MDEG                 45000L

/* =============================
 * Pitch trajectory
 * ============================= */

#define PITCH_RESPONSE_TIME_DEFAULT_MS      1000U
#define PITCH_RESPONSE_TIME_MIN_MS          200U
#define PITCH_RESPONSE_TIME_MAX_MS          5000U
#define PITCH_UPDATE_PERIOD_MS              20U

/* =============================
 * Yaw
 * ============================= */

#define YAW_AXIS_PULSES_PER_REV             1600U
/* Deprecated compatibility alias; production control uses YAW_AXIS_PULSES_PER_REV. */
#define YAW_PULSES_PER_REV                  YAW_AXIS_PULSES_PER_REV
#define ANGLE_MDEG_PER_REV                  360000L

/* INITIAL CONSERVATIVE BRING-UP LIMIT; not a motor or driver rating. */
#define YAW_STEP_FREQ_MIN_HZ                20U
#define YAW_STEP_FREQ_MAX_HZ                500U

#ifndef YAW_AXIS_SCALE_VERIFIED
#define YAW_AXIS_SCALE_VERIFIED             0U
#endif

/*
 * Mandatory cable-wrap limits for the no-slip-ring Yaw mechanism.
 * Establish Yaw 0 degrees with the Pitch cable in its neutral, untwisted
 * position. The total permitted travel is one revolution: -180 to +180.
 */
#define YAW_CABLE_LIMIT_MIN_MDEG            (-180000L)
#define YAW_CABLE_LIMIT_MAX_MDEG             180000L
#define YAW_CABLE_LIMIT_MIN_PULSES          (-((int32_t)(((-(int64_t)YAW_CABLE_LIMIT_MIN_MDEG) * \
                                                          YAW_AXIS_PULSES_PER_REV) / \
                                                         ANGLE_MDEG_PER_REV)))
#define YAW_CABLE_LIMIT_MAX_PULSES          ((int32_t)(((int64_t)YAW_CABLE_LIMIT_MAX_MDEG * \
                                                        YAW_AXIS_PULSES_PER_REV) / \
                                                       ANGLE_MDEG_PER_REV))

/* =============================
 * Debug and Ozone policy
 * ============================= */

#define DEBUG_SNAPSHOT_PERIOD_MS            20U
#define DEBUG_STATE_VERSION                 4U

#ifndef DEBUG_CONTROL_ENABLE
#if defined(DEBUG)
#define DEBUG_CONTROL_ENABLE                1
#else
#define DEBUG_CONTROL_ENABLE                0
#endif
#endif

#ifndef RAW_BENCH_COMMANDS_ENABLE
#if defined(DEBUG)
#define RAW_BENCH_COMMANDS_ENABLE           1
#else
#define RAW_BENCH_COMMANDS_ENABLE           0
#endif
#endif

#define DEBUG_ANGLE_INVALID_MDEG            INT32_MIN

typedef enum
{
    DEBUG_CMD_NONE = 0,
    DEBUG_CMD_SET_PITCH_MDEG,
    DEBUG_CMD_SET_PITCH_PULSE_US,
    DEBUG_CMD_SET_YAW_MDEG,
    DEBUG_CMD_SET_BOTH_MDEG,
    DEBUG_CMD_SET_YAW_ZERO,
    DEBUG_CMD_YAW_ENABLE,
    DEBUG_CMD_YAW_DISABLE,
    DEBUG_CMD_YAW_STOP,
    DEBUG_CMD_SET_PITCH_RESPONSE_MS
} DebugCommandType;

typedef enum
{
    DEBUG_RESULT_OK = 0,
    DEBUG_RESULT_DISABLED = -1,
    DEBUG_RESULT_UNKNOWN_COMMAND = -2,
    DEBUG_RESULT_INVALID_ARGUMENT = -3,
    DEBUG_RESULT_NOT_INITIALIZED = -4,
    DEBUG_RESULT_NOT_REFERENCED = -5,
    DEBUG_RESULT_LIMIT = -6,
    DEBUG_RESULT_BUSY = -7,
    DEBUG_RESULT_AXIS_DISABLED = -8,
    DEBUG_RESULT_DRIVER_ERROR = -9,
    DEBUG_RESULT_PARTIAL = -10,
    DEBUG_RESULT_INVALID_STATE = -11
} DebugCommandResult;

typedef struct
{
    int32_t target_mdeg;
    int32_t commanded_mdeg;
    int32_t servo_target_mdeg;
    uint32_t servo_pulse_us;
    int32_t measured_mdeg;
    uint32_t measurement_valid;
    uint32_t moving;
    uint32_t response_time_ms;
    uint32_t active_response_time_ms;
    uint32_t trajectory_elapsed_ms;
    int32_t soft_limit_min_mdeg;
    int32_t soft_limit_max_mdeg;
    uint32_t limit_reject_count;
    uint32_t tuning_reject_count;
    uint32_t servo_enabled;
    uint32_t calibration_valid;
    uint32_t raw_pulse_mode;
    int32_t status;
} PitchDebugState;

typedef struct
{
    int32_t target_mdeg;
    int32_t quantized_target_mdeg;
    int32_t commanded_mdeg;
    int32_t measured_mdeg;
    uint32_t measurement_valid;
    int32_t commanded_position_pulses;
    int32_t zero_offset_pulses;
    uint32_t remaining_pulses;
    uint32_t pulse_frequency_hz;
    uint32_t stepper_state;
    uint32_t enabled;
    uint32_t reference_state;
    int32_t soft_limit_min_mdeg;
    int32_t soft_limit_max_mdeg;
    uint32_t soft_limit_enabled;
    uint32_t limit_reject_count;
    int32_t status;
    /* Appended cable telemetry; legacy soft-limit fields above remain aliases. */
    int32_t cable_limit_min_mdeg;
    int32_t cable_limit_max_mdeg;
    int32_t cable_limit_min_pulses;
    int32_t cable_limit_max_pulses;
    int32_t cable_margin_to_min_mdeg;
    int32_t cable_margin_to_max_mdeg;
    uint32_t cable_remaining_negative_pulses;
    uint32_t cable_remaining_positive_pulses;
    uint32_t cable_limit_reject_count;
    uint32_t axis_pulses_per_rev;
    int32_t mdeg_per_pulse;
    uint32_t axis_scale_verified;
    uint32_t frequency_min_hz;
    uint32_t frequency_max_hz;
    uint32_t cable_margin_valid;
} YawDebugState;

typedef struct
{
    uint32_t snapshot_seq;
    uint32_t version;
    uint32_t heartbeat;
    uint32_t tick_ms;
    uint32_t app_health_flags;
    uint32_t last_debug_command;
    int32_t last_debug_result;
    PitchDebugState pitch;
    YawDebugState yaw;
} DebugState;

typedef struct
{
    uint32_t request_seq;
    uint32_t command;
    int32_t pitch_target_mdeg;
    int32_t yaw_target_mdeg;
    uint32_t yaw_frequency_hz;
    uint32_t pitch_pulse_us;
    uint32_t pitch_response_time_ms;
    uint32_t applied_seq;
    int32_t result;
} DebugCommand;

typedef struct
{
    uint32_t pitch_response_time_ms;
} DebugTuning;

typedef struct
{
    DebugState state;
    DebugCommand command;
    DebugTuning tuning;
} ControlDebugBlock;

/* Defined exactly once in Diagnostics/Src/debug_state.c. */
extern volatile ControlDebugBlock g_control_debug;

#if (SERVO_CALIBRATION_VALID != 0U) && (SERVO_CALIBRATION_VALID != 1U)
#error "SERVO_CALIBRATION_VALID must be 0 or 1"
#endif
#if (PITCH_SERVO_DIRECTION_SIGN != 1) && (PITCH_SERVO_DIRECTION_SIGN != -1)
#error "PITCH_SERVO_DIRECTION_SIGN must be +1 or -1"
#endif
#if (PITCH_SOFT_MIN_MDEG >= 0L)
#error "PITCH_SOFT_MIN_MDEG must be negative"
#endif
#if (PITCH_SOFT_MAX_MDEG <= 0L)
#error "PITCH_SOFT_MAX_MDEG must be positive"
#endif
#if ((PITCH_LEVEL_SERVO_MDEG + (PITCH_SERVO_DIRECTION_SIGN * PITCH_SOFT_MIN_MDEG)) < SERVO_MIN_ANGLE_MDEG) || \
    ((PITCH_LEVEL_SERVO_MDEG + (PITCH_SERVO_DIRECTION_SIGN * PITCH_SOFT_MIN_MDEG)) > SERVO_MAX_ANGLE_MDEG)
#error "Pitch lower software limit maps outside the Servo range"
#endif
#if ((PITCH_LEVEL_SERVO_MDEG + (PITCH_SERVO_DIRECTION_SIGN * PITCH_SOFT_MAX_MDEG)) < SERVO_MIN_ANGLE_MDEG) || \
    ((PITCH_LEVEL_SERVO_MDEG + (PITCH_SERVO_DIRECTION_SIGN * PITCH_SOFT_MAX_MDEG)) > SERVO_MAX_ANGLE_MDEG)
#error "Pitch upper software limit maps outside the Servo range"
#endif
#if (PITCH_RESPONSE_TIME_MIN_MS == 0U) || \
    (PITCH_RESPONSE_TIME_DEFAULT_MS < PITCH_RESPONSE_TIME_MIN_MS) || \
    (PITCH_RESPONSE_TIME_DEFAULT_MS > PITCH_RESPONSE_TIME_MAX_MS)
#error "Pitch response time configuration is invalid"
#endif
#if (PITCH_UPDATE_PERIOD_MS == 0U)
#error "PITCH_UPDATE_PERIOD_MS must be nonzero"
#endif
#if (YAW_AXIS_PULSES_PER_REV == 0U)
#error "YAW_AXIS_PULSES_PER_REV must be greater than zero"
#endif
#if (ANGLE_MDEG_PER_REV <= 0L)
#error "ANGLE_MDEG_PER_REV must be positive"
#endif
#if ((ANGLE_MDEG_PER_REV % YAW_AXIS_PULSES_PER_REV) != 0U)
#error "Yaw pulses per revolution must divide the angle revolution exactly"
#endif
#if (YAW_CABLE_LIMIT_MIN_MDEG >= 0L)
#error "Yaw cable minimum must be negative"
#endif
#if (YAW_CABLE_LIMIT_MAX_MDEG <= 0L)
#error "Yaw cable maximum must be positive"
#endif
#if (YAW_CABLE_LIMIT_MIN_MDEG >= YAW_CABLE_LIMIT_MAX_MDEG)
#error "Yaw cable limits must have MIN < MAX"
#endif
#if ((YAW_CABLE_LIMIT_MAX_MDEG - YAW_CABLE_LIMIT_MIN_MDEG) > ANGLE_MDEG_PER_REV)
#error "Yaw cable travel must not exceed one revolution"
#endif
#if (((-(YAW_CABLE_LIMIT_MIN_MDEG) * YAW_AXIS_PULSES_PER_REV) % ANGLE_MDEG_PER_REV) != 0L)
#error "Yaw minimum cable angle must map to an exact pulse count"
#endif
#if (((YAW_CABLE_LIMIT_MAX_MDEG * YAW_AXIS_PULSES_PER_REV) % ANGLE_MDEG_PER_REV) != 0L)
#error "Yaw maximum cable angle must map to an exact pulse count"
#endif
#if (YAW_STEP_FREQ_MIN_HZ < TB6600_STEP_FREQ_MIN_HZ)
#error "Yaw minimum rate is below the TB6600-supported minimum"
#endif
#if (YAW_STEP_FREQ_MAX_HZ > TB6600_STEP_FREQ_MAX_HZ)
#error "Yaw maximum rate exceeds the TB6600-supported maximum"
#endif
#if (YAW_STEP_FREQ_MIN_HZ > YAW_STEP_FREQ_MAX_HZ)
#error "Yaw frequency range is invalid"
#endif
#if (YAW_AXIS_SCALE_VERIFIED != 0U) && (YAW_AXIS_SCALE_VERIFIED != 1U)
#error "YAW_AXIS_SCALE_VERIFIED must be 0 or 1"
#endif

_Static_assert(YAW_CABLE_LIMIT_MIN_PULSES < 0,
               "Yaw cable minimum pulse limit must be negative");
_Static_assert(YAW_CABLE_LIMIT_MAX_PULSES > 0,
               "Yaw cable maximum pulse limit must be positive");
_Static_assert(YAW_CABLE_LIMIT_MIN_PULSES < YAW_CABLE_LIMIT_MAX_PULSES,
               "Yaw cable pulse limits must have MIN < MAX");
_Static_assert((-(int64_t)YAW_CABLE_LIMIT_MIN_MDEG * YAW_AXIS_PULSES_PER_REV) ==
               (-(int64_t)YAW_CABLE_LIMIT_MIN_PULSES * ANGLE_MDEG_PER_REV),
               "Yaw minimum cable pulse and angle limits are inconsistent");
_Static_assert(((int64_t)YAW_CABLE_LIMIT_MAX_MDEG * YAW_AXIS_PULSES_PER_REV) ==
               ((int64_t)YAW_CABLE_LIMIT_MAX_PULSES * ANGLE_MDEG_PER_REV),
               "Yaw maximum cable pulse and angle limits are inconsistent");
#if (DEBUG_CONTROL_ENABLE != 0) && (DEBUG_CONTROL_ENABLE != 1)
#error "DEBUG_CONTROL_ENABLE must be 0 or 1"
#endif
#if (RAW_BENCH_COMMANDS_ENABLE != 0) && (RAW_BENCH_COMMANDS_ENABLE != 1)
#error "RAW_BENCH_COMMANDS_ENABLE must be 0 or 1"
#endif

#endif /* CONTROL_DEBUG_CONFIG_H */

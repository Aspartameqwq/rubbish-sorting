#ifndef DEBUG_STATE_H
#define DEBUG_STATE_H

#include <stdint.h>

#include "project_config.h"

#define DEBUG_ANGLE_INVALID_MDEG INT32_MIN

typedef struct
{
    int32_t target_mdeg;
    int32_t commanded_mdeg;
    int32_t measured_mdeg;
    uint32_t measurement_valid;
    uint32_t pulse_us;
    uint32_t servo_enabled;
    uint32_t calibration_valid;
    uint32_t raw_pulse_mode;
    int32_t soft_limit_min_mdeg;
    int32_t soft_limit_max_mdeg;
    uint32_t soft_limit_enabled;
    uint32_t limit_reject_count;
    int32_t control_error_mdeg;
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
    int32_t control_error_mdeg;
    int32_t status;
} YawDebugState;

typedef struct
{
    uint32_t version;
    uint32_t heartbeat;
    uint32_t tick_ms;
    uint32_t app_health_flags;
    PitchDebugState pitch;
    YawDebugState yaw;
} DebugState;

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
    DEBUG_CMD_YAW_STOP
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
    DEBUG_RESULT_PARTIAL = -10
} DebugCommandResult;

typedef struct
{
    uint32_t request_seq;
    uint32_t command;
    int32_t pitch_target_mdeg;
    int32_t yaw_target_mdeg;
    uint32_t yaw_frequency_hz;
    uint32_t pitch_pulse_us;
    uint32_t applied_seq;
    int32_t result;
} DebugCommand;

/* Stable, externally linked symbols intended for Ozone's ELF-aware Watch window. */
extern volatile DebugState g_debug_state;
extern volatile DebugCommand g_debug_command;

void Debug_Init(void);
/* Called from the main loop; `now_ms` is supplied by the application layer. */
void Debug_Process(uint32_t now_ms, uint32_t app_health_flags);

#endif /* DEBUG_STATE_H */

#include "sort_task.h"

#include "control_debug_config.h"
#include "pitch_axis.h"
#include "project_config.h"
#include "protocol.h"
#include "stepper.h"
#include "yaw_axis.h"

#include <limits.h>
#include <stddef.h>

#if (SORT_ACTION_HISTORY_CAPACITY < 4U) || (SORT_ACTION_HISTORY_CAPACITY > 8U)
#error "SORT_ACTION_HISTORY_CAPACITY must be between four and eight entries"
#endif

#if (SORT_MECHANICAL_CALIBRATION_COMPLETE != 0U) && \
    (SORT_MECHANICAL_CALIBRATION_COMPLETE != 1U)
#error "SORT_MECHANICAL_CALIBRATION_COMPLETE must be 0 or 1"
#endif

volatile SortTask_t sort_task;
volatile uint8_t system_fault;
volatile uint8_t yaw_is_home;
volatile uint8_t yaw_is_at_target;
volatile uint8_t pitch_is_home;
volatile uint8_t pitch_is_at_target;

static const SortBoxConfig_t s_box_config[4] = {
    {1U, SORT_YAW_GROUP_13_MDEG, (PitchDumpDirection_t)SORT_BOX1_PITCH_DIRECTION},
    {2U, SORT_YAW_GROUP_24_MDEG, (PitchDumpDirection_t)SORT_BOX2_PITCH_DIRECTION},
    {3U, SORT_YAW_GROUP_13_MDEG, (PitchDumpDirection_t)SORT_BOX3_PITCH_DIRECTION},
    {4U, SORT_YAW_GROUP_24_MDEG, (PitchDumpDirection_t)SORT_BOX4_PITCH_DIRECTION}};

static SortActionRecord_t s_action_history[SORT_ACTION_HISTORY_CAPACITY];
static uint8_t s_action_history_next;
static uint32_t s_app_health_flags;

static void SortTask_ChangeState(SortState_t state, uint32_t now_ms)
{
    sort_task.state = state;
    sort_task.state_enter_tick = now_ms;
}

static bool SortTask_IsDirectionValid(PitchDumpDirection_t direction)
{
    return (direction == PITCH_DUMP_POSITIVE) || (direction == PITCH_DUMP_NEGATIVE);
}

bool SortTask_GetBoxConfig(uint8_t box_id, SortBoxConfig_t *config)
{
    if ((box_id < 1U) || (box_id > 4U) || (config == NULL))
    {
        return false;
    }

    *config = s_box_config[box_id - 1U];
    return true;
}

bool SortTask_ConfigIsValid(void)
{
    uint8_t index;
    int64_t yaw_group_13_mdeg;
    int64_t yaw_group_24_mdeg;
    uint64_t max_yaw_angle_mdeg;
    uint64_t max_yaw_pulses;
    uint64_t minimum_yaw_timeout_ms;

    if (SORT_MECHANICAL_CALIBRATION_COMPLETE != 1U)
    {
        return false;
    }
    if (!PitchAxis_IsCalibrationValid() || (YAW_AXIS_SCALE_VERIFIED == 0U))
    {
        return false;
    }
    if ((SORT_YAW_STEP_FREQUENCY_HZ < YAW_STEP_FREQ_MIN_HZ) ||
        (SORT_YAW_STEP_FREQUENCY_HZ > YAW_STEP_FREQ_MAX_HZ) ||
        (SORT_YAW_MOVE_TIMEOUT_MS == 0U) ||
        (SORT_PITCH_MOVE_TIMEOUT_MS < PITCH_RESPONSE_TIME_MAX_MS) ||
        (SORT_DUMP_HOLD_MS == 0U) ||
        (SORT_READY_HEARTBEAT_MS == 0U))
    {
        return false;
    }
    /* Existing axis coordinates define manual yaw zero and horizontal pitch as HOME. */
    if ((SORT_YAW_HOME_MDEG != 0L) || (SORT_PITCH_HOME_MDEG != 0L))
    {
        return false;
    }
    if ((SORT_PITCH_DUMP_ANGLE_MDEG <= 0L) ||
        (SORT_PITCH_DUMP_ANGLE_MDEG > PITCH_SOFT_MAX_MDEG) ||
        (SORT_PITCH_DUMP_ANGLE_MDEG > -PITCH_SOFT_MIN_MDEG))
    {
        return false;
    }
    if ((s_box_config[0].pitch_direction !=
         (PitchDumpDirection_t)(-s_box_config[2].pitch_direction)) ||
        (s_box_config[1].pitch_direction !=
         (PitchDumpDirection_t)(-s_box_config[3].pitch_direction)))
    {
        return false;
    }

    for (index = 0U; index < 4U; index++)
    {
        if ((s_box_config[index].box_id != (uint8_t)(index + 1U)) ||
            !SortTask_IsDirectionValid(s_box_config[index].pitch_direction) ||
            (s_box_config[index].yaw_target_mdeg < YAW_CABLE_LIMIT_MIN_MDEG) ||
            (s_box_config[index].yaw_target_mdeg > YAW_CABLE_LIMIT_MAX_MDEG))
        {
            return false;
        }
    }

    yaw_group_13_mdeg = SORT_YAW_GROUP_13_MDEG;
    yaw_group_24_mdeg = SORT_YAW_GROUP_24_MDEG;
    if (yaw_group_13_mdeg < 0)
    {
        yaw_group_13_mdeg = -yaw_group_13_mdeg;
    }
    if (yaw_group_24_mdeg < 0)
    {
        yaw_group_24_mdeg = -yaw_group_24_mdeg;
    }
    max_yaw_angle_mdeg = (uint64_t)((yaw_group_13_mdeg > yaw_group_24_mdeg)
                                        ? yaw_group_13_mdeg
                                        : yaw_group_24_mdeg);
    max_yaw_pulses = (max_yaw_angle_mdeg * YAW_AXIS_PULSES_PER_REV +
                      (uint64_t)ANGLE_MDEG_PER_REV - 1U) /
                     (uint64_t)ANGLE_MDEG_PER_REV;
    minimum_yaw_timeout_ms =
        ((max_yaw_pulses * 1000U + SORT_YAW_STEP_FREQUENCY_HZ - 1U) /
         SORT_YAW_STEP_FREQUENCY_HZ) + STEPPER_DIRECTION_SETUP_MS +
        SORT_YAW_MOVE_TIMEOUT_MARGIN_MS;
    if ((uint64_t)SORT_YAW_MOVE_TIMEOUT_MS < minimum_yaw_timeout_ms)
    {
        return false;
    }
    return true;
}

static bool SortTask_HardwareFaultActive(void)
{
    return (s_app_health_flags != 0U) ||
           (Stepper_GetState() == STEPPER_STATE_FAULT) ||
           (YawAxis_GetLastStatus() == YAW_AXIS_STATUS_DRIVER_ERROR) ||
           (PitchAxis_GetLastStatus() == PITCH_AXIS_STATUS_DRIVER_ERROR);
}

bool SortTask_HasSystemFault(void)
{
    const bool fault_active = SortTask_HardwareFaultActive() ||
                              (sort_task.state == SORT_STATE_FAULT);
    system_fault = fault_active ? 1U : 0U;
    return fault_active;
}

static void SortTask_UpdateTelemetry(void)
{
    const int32_t commanded_yaw = YawAxis_GetCommandedMilliDeg();
    const int32_t quantized_yaw_target = YawAxis_GetQuantizedTargetMilliDeg();
    const int32_t active_pitch_target = PitchAxis_GetTargetMilliDeg();
    const bool yaw_position_valid =
        (YawAxis_GetReferenceState() != YAW_REFERENCE_INVALID) &&
        (commanded_yaw != INT32_MIN);
    const bool commanded_pitch_valid = PitchAxis_IsCommandedAngleValid();

    yaw_is_at_target = (yaw_position_valid && !YawAxis_IsBusy() &&
                        (quantized_yaw_target != INT32_MIN) &&
                        (commanded_yaw == quantized_yaw_target))
                           ? 1U
                           : 0U;
    yaw_is_home = (yaw_position_valid && !YawAxis_IsBusy() &&
                   (commanded_yaw == SORT_YAW_HOME_MDEG) &&
                   (quantized_yaw_target == SORT_YAW_HOME_MDEG))
                      ? 1U
                      : 0U;

    pitch_is_at_target = (commanded_pitch_valid && !PitchAxis_IsMoving() &&
                          (active_pitch_target != INT32_MIN) &&
                          (PitchAxis_GetCommandedMilliDeg() == active_pitch_target))
                             ? 1U
                             : 0U;
    pitch_is_home = (pitch_is_at_target != 0U &&
                     active_pitch_target == SORT_PITCH_HOME_MDEG)
                        ? 1U
                        : 0U;
    (void)SortTask_HasSystemFault();
}

void SortTask_Init(uint32_t app_health_flags)
{
    uint8_t index;

    sort_task.state = SORT_STATE_IDLE;
    sort_task.action_id = 0U;
    sort_task.box = 0U;
    sort_task.yaw_target_mdeg = 0;
    sort_task.pitch_direction = PITCH_DUMP_UNCALIBRATED;
    sort_task.pitch_target_mdeg = 0;
    sort_task.state_enter_tick = 0U;
    sort_task.result = 0U;
    sort_task.action_valid = false;
    sort_task.action_completed = false;
    sort_task.fault_code = SORT_FAULT_NONE;

    system_fault = 0U;
    yaw_is_home = 0U;
    yaw_is_at_target = 0U;
    pitch_is_home = 0U;
    pitch_is_at_target = 0U;

    for (index = 0U; index < SORT_ACTION_HISTORY_CAPACITY; index++)
    {
        s_action_history[index].valid = false;
        s_action_history[index].action_id = 0U;
        s_action_history[index].box = 0U;
        s_action_history[index].result = 0U;
        s_action_history[index].completed = false;
    }
    s_action_history_next = 0U;
    s_app_health_flags = app_health_flags;
    SortTask_UpdateTelemetry();
}

bool SortTask_FindAction(uint32_t action_id, SortActionRecord_t *record)
{
    uint8_t index;

    for (index = 0U; index < SORT_ACTION_HISTORY_CAPACITY; index++)
    {
        if (s_action_history[index].valid &&
            (s_action_history[index].action_id == action_id))
        {
            if (record != NULL)
            {
                *record = s_action_history[index];
            }
            return true;
        }
    }
    return false;
}

static void SortTask_StoreAction(uint32_t action_id, uint8_t box_id)
{
    SortActionRecord_t *record = &s_action_history[s_action_history_next];

    record->valid = true;
    record->action_id = action_id;
    record->box = box_id;
    record->result = 0U;
    record->completed = false;
    s_action_history_next++;
    if (s_action_history_next >= SORT_ACTION_HISTORY_CAPACITY)
    {
        s_action_history_next = 0U;
    }
}

static void SortTask_MarkActionCompleted(uint32_t action_id, uint8_t result)
{
    uint8_t index;

    for (index = 0U; index < SORT_ACTION_HISTORY_CAPACITY; index++)
    {
        if (s_action_history[index].valid &&
            (s_action_history[index].action_id == action_id))
        {
            s_action_history[index].result = result;
            s_action_history[index].completed = true;
            return;
        }
    }
}

SortAcceptStatus_t SortTask_AcceptAction(uint32_t action_id, uint8_t box_id)
{
    SortBoxConfig_t config;
    SortActionRecord_t previous_record;
    int64_t target_pitch_mdeg;
    YawAxisStatus yaw_status;
    PitchAxisStatus pitch_status;

    if ((box_id < 1U) || (box_id > 4U))
    {
        return SORT_ACCEPT_BAD_BOX;
    }
    if (SortTask_FindAction(action_id, &previous_record))
    {
        return SORT_ACCEPT_DUPLICATE;
    }
    if (SortTask_HasSystemFault() || (sort_task.state == SORT_STATE_FAULT) ||
        !SortTask_ConfigIsValid())
    {
        return SORT_ACCEPT_FAULT;
    }
    if (SortTask_IsBusy() || YawAxis_IsBusy() || PitchAxis_IsMoving())
    {
        return SORT_ACCEPT_BUSY;
    }
    if (!SortTask_IsReady() || !SortTask_GetBoxConfig(box_id, &config))
    {
        return SORT_ACCEPT_FAULT;
    }

    yaw_status = YawAxis_ValidateTargetMilliDeg(config.yaw_target_mdeg,
                                                 SORT_YAW_STEP_FREQUENCY_HZ);
    if (yaw_status == YAW_AXIS_STATUS_BUSY)
    {
        return SORT_ACCEPT_BUSY;
    }
    if (yaw_status != YAW_AXIS_STATUS_OK)
    {
        return SORT_ACCEPT_FAULT;
    }

    target_pitch_mdeg = (int64_t)SORT_PITCH_DUMP_ANGLE_MDEG *
                        (int32_t)config.pitch_direction;
    if ((target_pitch_mdeg < INT32_MIN) || (target_pitch_mdeg > INT32_MAX))
    {
        return SORT_ACCEPT_FAULT;
    }
    pitch_status = PitchAxis_ValidateTargetMilliDeg((int32_t)target_pitch_mdeg);
    if (pitch_status != PITCH_AXIS_STATUS_OK)
    {
        return SORT_ACCEPT_FAULT;
    }

    sort_task.action_id = action_id;
    sort_task.box = box_id;
    sort_task.yaw_target_mdeg = config.yaw_target_mdeg;
    sort_task.pitch_direction = config.pitch_direction;
    sort_task.pitch_target_mdeg = (int32_t)target_pitch_mdeg;
    sort_task.result = 0U;
    sort_task.action_valid = true;
    sort_task.action_completed = false;
    sort_task.fault_code = SORT_FAULT_NONE;
    sort_task.state_enter_tick = 0U;
    SortTask_StoreAction(action_id, box_id);
    SortTask_UpdateTelemetry();
    return SORT_ACCEPT_ACCEPTED;
}

void SortTask_StartAcceptedAction(uint32_t now_ms)
{
    if (sort_task.action_valid && !sort_task.action_completed &&
        (sort_task.state == SORT_STATE_IDLE))
    {
        SortTask_ChangeState(SORT_STATE_YAW_MOVE, now_ms);
    }
}

bool SortTask_IsBusy(void)
{
    if (sort_task.action_valid && !sort_task.action_completed)
    {
        return true;
    }
    return (sort_task.state != SORT_STATE_IDLE) &&
           (sort_task.state != SORT_STATE_FAULT);
}

bool SortTask_IsReady(void)
{
    SortTask_UpdateTelemetry();
    return (sort_task.state == SORT_STATE_IDLE) && !sort_task.action_valid &&
           !SortTask_HasSystemFault() && SortTask_ConfigIsValid() &&
           YawAxis_IsEnabled() &&
           (YawAxis_GetReferenceState() != YAW_REFERENCE_INVALID) &&
           (yaw_is_home != 0U) && (yaw_is_at_target != 0U) &&
           (pitch_is_home != 0U) &&
           !YawAxis_IsBusy() && !PitchAxis_IsMoving() &&
           PitchAxis_IsServoEnabled();
}

static bool SortTask_YawAtTarget(void)
{
    const int32_t commanded_mdeg = YawAxis_GetCommandedMilliDeg();
    const int32_t quantized_target_mdeg = YawAxis_GetQuantizedTargetMilliDeg();

    return (YawAxis_GetReferenceState() != YAW_REFERENCE_INVALID) &&
           (commanded_mdeg != INT32_MIN) &&
           (quantized_target_mdeg != INT32_MIN) && !YawAxis_IsBusy() &&
           (commanded_mdeg == quantized_target_mdeg);
}

static bool SortTask_PitchAtTarget(int32_t target_mdeg)
{
    return !PitchAxis_IsMoving() &&
           (PitchAxis_GetTargetMilliDeg() == target_mdeg) &&
           (PitchAxis_GetCommandedMilliDeg() == target_mdeg);
}

static void SortTask_EnterFault(SortFaultCode_t fault_code, uint32_t now_ms)
{
    if (YawAxis_IsBusy())
    {
        (void)YawAxis_Stop();
    }

    sort_task.fault_code = (uint32_t)fault_code;
    sort_task.result = 1U;
    sort_task.action_completed = true;
    SortTask_ChangeState(SORT_STATE_FAULT, now_ms);
    if (sort_task.action_valid)
    {
        SortTask_MarkActionCompleted(sort_task.action_id, 1U);
        Protocol_SendDone(sort_task.action_id, 1U);
    }
    system_fault = 1U;
    SortTask_UpdateTelemetry();
}

static void SortTask_Complete(uint32_t now_ms)
{
    if (!SortTask_YawAtTarget() ||
        (YawAxis_GetCommandedMilliDeg() != SORT_YAW_HOME_MDEG) ||
        !SortTask_PitchAtTarget(SORT_PITCH_HOME_MDEG) ||
        SortTask_HardwareFaultActive())
    {
        SortTask_EnterFault(SORT_FAULT_SYSTEM, now_ms);
        return;
    }

    sort_task.result = 0U;
    sort_task.action_completed = true;
    SortTask_MarkActionCompleted(sort_task.action_id, 0U);
    Protocol_SendDone(sort_task.action_id, 0U);
    sort_task.action_valid = false;
    SortTask_ChangeState(SORT_STATE_IDLE, now_ms);
    SortTask_UpdateTelemetry();
}

void SortTask_Process(uint32_t now_ms)
{
    SortTask_UpdateTelemetry();

    if (SortTask_IsBusy() && SortTask_HardwareFaultActive())
    {
        SortTask_EnterFault(SORT_FAULT_SYSTEM, now_ms);
        return;
    }

    switch (sort_task.state)
    {
        case SORT_STATE_YAW_MOVE:
            if (!SortTask_ConfigIsValid() ||
                (YawAxis_ValidateTargetMilliDeg(sort_task.yaw_target_mdeg,
                                                 SORT_YAW_STEP_FREQUENCY_HZ) !=
                 YAW_AXIS_STATUS_OK) ||
                (YawAxis_SetTargetMilliDeg(sort_task.yaw_target_mdeg,
                                            SORT_YAW_STEP_FREQUENCY_HZ) !=
                 YAW_AXIS_STATUS_OK))
            {
                SortTask_EnterFault(SORT_FAULT_YAW_TARGET, now_ms);
                return;
            }
            SortTask_ChangeState(SORT_STATE_YAW_WAIT, now_ms);
            break;

        case SORT_STATE_YAW_WAIT:
            if (SortTask_YawAtTarget())
            {
                SortTask_ChangeState(SORT_STATE_PITCH_DUMP, now_ms);
            }
            else if ((uint32_t)(now_ms - sort_task.state_enter_tick) >=
                     SORT_YAW_MOVE_TIMEOUT_MS)
            {
                SortTask_EnterFault(SORT_FAULT_YAW_MOVE_TIMEOUT, now_ms);
            }
            break;

        case SORT_STATE_PITCH_DUMP:
            if (PitchAxis_SetTargetMilliDeg(sort_task.pitch_target_mdeg) !=
                PITCH_AXIS_STATUS_OK)
            {
                SortTask_EnterFault(SORT_FAULT_PITCH_TARGET, now_ms);
                return;
            }
            SortTask_ChangeState(SORT_STATE_PITCH_DUMP_WAIT, now_ms);
            break;

        case SORT_STATE_PITCH_DUMP_WAIT:
            if (SortTask_PitchAtTarget(sort_task.pitch_target_mdeg))
            {
                SortTask_ChangeState(SORT_STATE_DUMP_HOLD, now_ms);
            }
            else if ((uint32_t)(now_ms - sort_task.state_enter_tick) >=
                     SORT_PITCH_MOVE_TIMEOUT_MS)
            {
                SortTask_EnterFault(SORT_FAULT_PITCH_DUMP_TIMEOUT, now_ms);
            }
            break;

        case SORT_STATE_DUMP_HOLD:
            if ((uint32_t)(now_ms - sort_task.state_enter_tick) >= SORT_DUMP_HOLD_MS)
            {
                SortTask_ChangeState(SORT_STATE_PITCH_RETURN, now_ms);
            }
            break;

        case SORT_STATE_PITCH_RETURN:
            if (PitchAxis_SetTargetMilliDeg(SORT_PITCH_HOME_MDEG) != PITCH_AXIS_STATUS_OK)
            {
                SortTask_EnterFault(SORT_FAULT_PITCH_TARGET, now_ms);
                return;
            }
            SortTask_ChangeState(SORT_STATE_PITCH_RETURN_WAIT, now_ms);
            break;

        case SORT_STATE_PITCH_RETURN_WAIT:
            if (SortTask_PitchAtTarget(SORT_PITCH_HOME_MDEG))
            {
                SortTask_ChangeState(SORT_STATE_YAW_RETURN, now_ms);
            }
            else if ((uint32_t)(now_ms - sort_task.state_enter_tick) >=
                     SORT_PITCH_MOVE_TIMEOUT_MS)
            {
                SortTask_EnterFault(SORT_FAULT_PITCH_RETURN_TIMEOUT, now_ms);
            }
            break;

        case SORT_STATE_YAW_RETURN:
            if ((PitchAxis_GetTargetMilliDeg() != SORT_PITCH_HOME_MDEG) ||
                PitchAxis_IsMoving() ||
                (PitchAxis_GetCommandedMilliDeg() != SORT_PITCH_HOME_MDEG) ||
                (YawAxis_SetTargetMilliDeg(SORT_YAW_HOME_MDEG,
                                           SORT_YAW_STEP_FREQUENCY_HZ) !=
                 YAW_AXIS_STATUS_OK))
            {
                SortTask_EnterFault(SORT_FAULT_YAW_TARGET, now_ms);
                return;
            }
            SortTask_ChangeState(SORT_STATE_YAW_RETURN_WAIT, now_ms);
            break;

        case SORT_STATE_YAW_RETURN_WAIT:
            if (SortTask_YawAtTarget() &&
                (YawAxis_GetCommandedMilliDeg() == SORT_YAW_HOME_MDEG) &&
                SortTask_PitchAtTarget(SORT_PITCH_HOME_MDEG))
            {
                SortTask_ChangeState(SORT_STATE_COMPLETE, now_ms);
            }
            else if ((uint32_t)(now_ms - sort_task.state_enter_tick) >=
                     SORT_YAW_MOVE_TIMEOUT_MS)
            {
                SortTask_EnterFault(SORT_FAULT_YAW_RETURN_TIMEOUT, now_ms);
            }
            break;

        case SORT_STATE_COMPLETE:
            SortTask_Complete(now_ms);
            break;

        case SORT_STATE_IDLE:
        case SORT_STATE_FAULT:
        default:
            break;
    }

    SortTask_UpdateTelemetry();
}

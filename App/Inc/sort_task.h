#ifndef SORT_TASK_H
#define SORT_TASK_H

#include <stdbool.h>
#include <stdint.h>

typedef enum
{
    SORT_STATE_IDLE = 0,
    SORT_STATE_YAW_MOVE,
    SORT_STATE_YAW_WAIT,
    SORT_STATE_PITCH_DUMP,
    SORT_STATE_PITCH_DUMP_WAIT,
    SORT_STATE_DUMP_HOLD,
    SORT_STATE_PITCH_RETURN,
    SORT_STATE_PITCH_RETURN_WAIT,
    SORT_STATE_YAW_RETURN,
    SORT_STATE_YAW_RETURN_WAIT,
    SORT_STATE_COMPLETE,
    SORT_STATE_FAULT
} SortState_t;

typedef enum
{
    PITCH_DUMP_UNCALIBRATED = 0,
    PITCH_DUMP_POSITIVE = 1,
    PITCH_DUMP_NEGATIVE = -1
} PitchDumpDirection_t;

typedef struct
{
    uint8_t box_id;
    int32_t yaw_target_mdeg;
    PitchDumpDirection_t pitch_direction;
} SortBoxConfig_t;

typedef enum
{
    SORT_FAULT_NONE = 0,
    SORT_FAULT_YAW_MOVE_TIMEOUT,
    SORT_FAULT_PITCH_DUMP_TIMEOUT,
    SORT_FAULT_PITCH_RETURN_TIMEOUT,
    SORT_FAULT_YAW_RETURN_TIMEOUT,
    SORT_FAULT_YAW_TARGET,
    SORT_FAULT_PITCH_TARGET,
    SORT_FAULT_SYSTEM
} SortFaultCode_t;

typedef struct
{
    SortState_t state;
    uint32_t action_id;
    uint8_t box;
    int32_t yaw_target_mdeg;
    PitchDumpDirection_t pitch_direction;
    int32_t pitch_target_mdeg;
    uint32_t state_enter_tick;
    uint8_t result;
    bool action_valid;
    bool action_completed;
    uint32_t fault_code;
} SortTask_t;

typedef struct
{
    bool valid;
    uint32_t action_id;
    uint8_t box;
    uint8_t result;
    bool completed;
} SortActionRecord_t;

typedef enum
{
    SORT_ACCEPT_ACCEPTED = 0,
    SORT_ACCEPT_BAD_BOX,
    SORT_ACCEPT_BUSY,
    SORT_ACCEPT_FAULT,
    SORT_ACCEPT_DUPLICATE
} SortAcceptStatus_t;

/* Ozone watch symbols. Angle values use signed millidegrees. */
extern volatile SortTask_t sort_task;
extern volatile uint8_t system_fault;
extern volatile uint8_t yaw_is_home;
extern volatile uint8_t yaw_is_at_target;
extern volatile uint8_t pitch_is_home;
extern volatile uint8_t pitch_is_at_target;

void SortTask_Init(uint32_t app_health_flags);
void SortTask_Process(uint32_t now_ms);
bool SortTask_ConfigIsValid(void);
bool SortTask_GetBoxConfig(uint8_t box_id, SortBoxConfig_t *config);
SortAcceptStatus_t SortTask_AcceptAction(uint32_t action_id, uint8_t box_id);
void SortTask_StartAcceptedAction(uint32_t now_ms);
bool SortTask_FindAction(uint32_t action_id, SortActionRecord_t *record);
bool SortTask_IsBusy(void);
bool SortTask_HasSystemFault(void);
bool SortTask_IsReady(void);

#endif /* SORT_TASK_H */

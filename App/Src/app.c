#include "app.h"

#include "debug_state.h"
#include "hc04.h"
#include "pitch_axis.h"
#include "protocol.h"
#include "servo.h"
#include "stepper.h"
#include "tb6600.h"
#include "yaw_axis.h"

#include <stdbool.h>

static bool s_init_attempted;
static AppHealthFlags s_health_flags;

void App_Init(void)
{
    s_health_flags = APP_HEALTH_OK;

    if (Servo_Init() != SERVO_STATUS_OK)
    {
        s_health_flags |= APP_HEALTH_SERVO_INIT_ERROR;
    }
    if (HC04_Init() != HC04_STATUS_OK)
    {
        s_health_flags |= APP_HEALTH_HC04_INIT_ERROR;
    }
    if (TB6600_Init() != TB6600_STATUS_OK)
    {
        s_health_flags |= APP_HEALTH_TB6600_INIT_ERROR;
    }
    if (Stepper_Init() != STEPPER_STATUS_OK)
    {
        s_health_flags |= APP_HEALTH_STEPPER_INIT_ERROR;
    }
    if (PitchAxis_Init() != PITCH_AXIS_STATUS_OK)
    {
        s_health_flags |= APP_HEALTH_PITCH_AXIS_INIT_ERROR;
    }
    if (YawAxis_Init() != YAW_AXIS_STATUS_OK)
    {
        s_health_flags |= APP_HEALTH_YAW_AXIS_INIT_ERROR;
    }

    Protocol_Init();
    Debug_Init();
    s_init_attempted = true;
}

void App_Process(void)
{
    if (!s_init_attempted)
    {
        return;
    }

    HC04_Process();
    YawAxis_Process();
    Protocol_Process();
    Debug_Process(TB6600_GetTickMs(), s_health_flags);
}

AppHealthFlags App_GetHealthFlags(void)
{
    return s_health_flags;
}

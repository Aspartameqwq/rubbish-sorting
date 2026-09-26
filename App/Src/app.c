#include "app.h"

#include "hc04.h"
#include "protocol.h"
#include "servo.h"
#include "stepper.h"
#include "tb6600.h"

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

    Protocol_Init();
    s_init_attempted = true;
}

void App_Process(void)
{
    if (!s_init_attempted)
    {
        return;
    }

    HC04_Process();
    Stepper_Process();
    Protocol_Process();
}

AppHealthFlags App_GetHealthFlags(void)
{
    return s_health_flags;
}

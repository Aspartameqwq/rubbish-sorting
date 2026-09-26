#include "app.h"

#include "hc04.h"
#include "protocol.h"
#include "servo.h"

#include <stdbool.h>

static bool s_initialized;

void App_Init(void)
{
    (void)Servo_Init();
    (void)HC04_Init();
    Protocol_Init();
    s_initialized = true;
}

void App_Process(void)
{
    if (!s_initialized)
    {
        return;
    }

    HC04_Process();
    Protocol_Process();
}

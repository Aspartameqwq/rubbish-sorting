#ifndef APP_H
#define APP_H

#include <stdint.h>

typedef uint32_t AppHealthFlags;

enum
{
    APP_HEALTH_OK = 0U,
    APP_HEALTH_SERVO_INIT_ERROR = (1U << 0),
    APP_HEALTH_HC04_INIT_ERROR = (1U << 1),
    APP_HEALTH_TB6600_INIT_ERROR = (1U << 2),
    APP_HEALTH_STEPPER_INIT_ERROR = (1U << 3),
    APP_HEALTH_PITCH_AXIS_INIT_ERROR = (1U << 4),
    APP_HEALTH_YAW_AXIS_INIT_ERROR = (1U << 5)
};

void App_Init(void);
void App_Process(void);
/* Bitmask of initialization errors observed by App_Init(). */
AppHealthFlags App_GetHealthFlags(void);

#endif /* APP_H */

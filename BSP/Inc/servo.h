#ifndef SERVO_H
#define SERVO_H

#include <stdbool.h>
#include <stdint.h>

typedef enum
{
    SERVO_STATUS_OK = 0,
    SERVO_STATUS_INVALID_ARGUMENT,
    SERVO_STATUS_NOT_INITIALIZED,
    SERVO_STATUS_CONFIGURATION_ERROR,
    SERVO_STATUS_HAL_ERROR
} ServoStatus;

ServoStatus Servo_Init(void);
ServoStatus Servo_SetAngle(uint16_t angle_deg);
ServoStatus Servo_SetPulseUs(uint16_t pulse_us);
/* Meaningful only while Servo_IsAngleValid() is true. */
uint16_t Servo_GetAngle(void);
uint16_t Servo_GetPulseUs(void);
bool Servo_IsAngleValid(void);
bool Servo_IsInitialized(void);
bool Servo_IsEnabled(void);
ServoStatus Servo_Enable(void);
ServoStatus Servo_Disable(void);

#endif /* SERVO_H */

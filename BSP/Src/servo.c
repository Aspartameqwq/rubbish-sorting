#include "servo.h"

#include "project_config.h"
#include "tim.h"

#if SERVO_MAX_ANGLE_DEG <= SERVO_MIN_ANGLE_DEG
#error "Servo angle range must be positive"
#endif

#if SERVO_MAX_PULSE_US <= SERVO_MIN_PULSE_US
#error "Servo pulse range must be positive"
#endif

#if (SERVO_CENTER_PULSE_US < SERVO_MIN_PULSE_US) || \
    (SERVO_CENTER_PULSE_US > SERVO_MAX_PULSE_US)
#error "Servo center pulse must be within the configured pulse range"
#endif

static bool s_initialized;
static bool s_enabled;
static uint16_t s_angle_deg;
static uint16_t s_pulse_us;

static uint16_t Servo_AngleFromPulse(uint16_t pulse_us)
{
    const uint32_t pulse_span = (uint32_t)SERVO_MAX_PULSE_US - SERVO_MIN_PULSE_US;
    const uint32_t angle_span = (uint32_t)SERVO_MAX_ANGLE_DEG - SERVO_MIN_ANGLE_DEG;
    const uint32_t pulse_offset = (uint32_t)pulse_us - SERVO_MIN_PULSE_US;
    const uint32_t angle_offset = (pulse_offset * angle_span + (pulse_span / 2U)) / pulse_span;

    return (uint16_t)((uint32_t)SERVO_MIN_ANGLE_DEG + angle_offset);
}

ServoStatus Servo_Init(void)
{
    if (s_initialized)
    {
        return SERVO_STATUS_OK;
    }

    if ((htim2.Instance != TIM2) ||
        (htim2.Init.Prescaler != 71U) ||
        (htim2.Init.Period != 19999U) ||
        (SERVO_MAX_PULSE_US >= (htim2.Init.Period + 1U)))
    {
        return SERVO_STATUS_CONFIGURATION_ERROR;
    }

    __HAL_TIM_SET_COMPARE(&htim2, TIM_CHANNEL_1, SERVO_CENTER_PULSE_US);
    if (HAL_TIM_PWM_Start(&htim2, TIM_CHANNEL_1) != HAL_OK)
    {
        return SERVO_STATUS_HAL_ERROR;
    }

    s_angle_deg = SERVO_CENTER_ANGLE_DEG;
    s_pulse_us = SERVO_CENTER_PULSE_US;
    s_enabled = true;
    s_initialized = true;
    return SERVO_STATUS_OK;
}

ServoStatus Servo_SetAngle(uint16_t angle_deg)
{
    uint32_t pulse_offset;
    uint32_t angle_span;
    uint32_t pulse_span;
    uint16_t pulse_us;

    if (!s_initialized)
    {
        return SERVO_STATUS_NOT_INITIALIZED;
    }
    if (angle_deg > SERVO_MAX_ANGLE_DEG)
    {
        return SERVO_STATUS_INVALID_ARGUMENT;
    }

    angle_span = (uint32_t)SERVO_MAX_ANGLE_DEG - SERVO_MIN_ANGLE_DEG;
    pulse_span = (uint32_t)SERVO_MAX_PULSE_US - SERVO_MIN_PULSE_US;
    pulse_offset = (((uint32_t)angle_deg - SERVO_MIN_ANGLE_DEG) * pulse_span) / angle_span;
    pulse_us = (uint16_t)((uint32_t)SERVO_MIN_PULSE_US + pulse_offset);

    return Servo_SetPulseUs(pulse_us);
}

ServoStatus Servo_SetPulseUs(uint16_t pulse_us)
{
    if (!s_initialized)
    {
        return SERVO_STATUS_NOT_INITIALIZED;
    }
    if ((pulse_us < SERVO_MIN_PULSE_US) || (pulse_us > SERVO_MAX_PULSE_US) ||
        (pulse_us >= (htim2.Init.Period + 1U)))
    {
        return SERVO_STATUS_INVALID_ARGUMENT;
    }

    __HAL_TIM_SET_COMPARE(&htim2, TIM_CHANNEL_1, pulse_us);
    s_pulse_us = pulse_us;
    s_angle_deg = Servo_AngleFromPulse(pulse_us);
    return SERVO_STATUS_OK;
}

uint16_t Servo_GetAngle(void)
{
    return s_angle_deg;
}

uint16_t Servo_GetPulseUs(void)
{
    return s_pulse_us;
}

bool Servo_IsInitialized(void)
{
    return s_initialized;
}

bool Servo_IsEnabled(void)
{
    return s_enabled;
}

ServoStatus Servo_Enable(void)
{
    if (!s_initialized)
    {
        return SERVO_STATUS_NOT_INITIALIZED;
    }
    if (s_enabled)
    {
        return SERVO_STATUS_OK;
    }

    if (HAL_TIM_PWM_Start(&htim2, TIM_CHANNEL_1) != HAL_OK)
    {
        return SERVO_STATUS_HAL_ERROR;
    }
    s_enabled = true;
    return SERVO_STATUS_OK;
}

ServoStatus Servo_Disable(void)
{
    if (!s_initialized)
    {
        return SERVO_STATUS_NOT_INITIALIZED;
    }
    if (!s_enabled)
    {
        return SERVO_STATUS_OK;
    }

    if (HAL_TIM_PWM_Stop(&htim2, TIM_CHANNEL_1) != HAL_OK)
    {
        return SERVO_STATUS_HAL_ERROR;
    }
    s_enabled = false;
    return SERVO_STATUS_OK;
}

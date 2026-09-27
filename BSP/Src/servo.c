#include "servo.h"

#include "control_debug_config.h"
#include "tim.h"

#include <limits.h>
#include <stddef.h>

#if (SERVO_CENTER_ANGLE_MDEG <= SERVO_MIN_ANGLE_MDEG) || \
    (SERVO_CENTER_ANGLE_MDEG >= SERVO_MAX_ANGLE_MDEG)
#error "Servo center angle must be strictly inside the configured angle range"
#endif

#if (SERVO_CENTER_PULSE_US <= SERVO_MIN_PULSE_US) || \
    (SERVO_CENTER_PULSE_US >= SERVO_MAX_PULSE_US)
#error "Servo center pulse must be strictly inside the configured pulse range"
#endif

static bool s_initialized;
static bool s_enabled;
static bool s_angle_valid;
static int32_t s_target_angle_mdeg;
static int32_t s_commanded_angle_mdeg;
static uint16_t s_pulse_us;

static uint16_t Servo_AngleToPulse(int32_t angle_mdeg)
{
    int64_t angle_start_mdeg;
    int64_t angle_end_mdeg;
    int64_t pulse_start_us;
    int64_t pulse_end_us;
    int64_t numerator;
    int64_t denominator;
    int64_t pulse_us;

    if (angle_mdeg <= SERVO_CENTER_ANGLE_MDEG)
    {
        angle_start_mdeg = (int64_t)SERVO_MIN_ANGLE_DEG * 1000;
        angle_end_mdeg = SERVO_CENTER_ANGLE_MDEG;
        pulse_start_us = SERVO_MIN_PULSE_US;
        pulse_end_us = SERVO_CENTER_PULSE_US;
    }
    else
    {
        angle_start_mdeg = SERVO_CENTER_ANGLE_MDEG;
        angle_end_mdeg = (int64_t)SERVO_MAX_ANGLE_DEG * 1000;
        pulse_start_us = SERVO_CENTER_PULSE_US;
        pulse_end_us = SERVO_MAX_PULSE_US;
    }

    numerator = ((int64_t)angle_mdeg - angle_start_mdeg) *
                (pulse_end_us - pulse_start_us);
    denominator = angle_end_mdeg - angle_start_mdeg;
    pulse_us = pulse_start_us + ((numerator + (denominator / 2)) / denominator);
    return (uint16_t)pulse_us;
}

static int32_t Servo_PulseToAngle(uint16_t pulse_us)
{
    int64_t pulse_start_us;
    int64_t pulse_end_us;
    int64_t angle_start_mdeg;
    int64_t angle_end_mdeg;
    int64_t numerator;
    int64_t denominator;
    int64_t angle_mdeg;

    if (pulse_us <= SERVO_CENTER_PULSE_US)
    {
        pulse_start_us = SERVO_MIN_PULSE_US;
        pulse_end_us = SERVO_CENTER_PULSE_US;
        angle_start_mdeg = (int64_t)SERVO_MIN_ANGLE_DEG * 1000;
        angle_end_mdeg = SERVO_CENTER_ANGLE_MDEG;
    }
    else
    {
        pulse_start_us = SERVO_CENTER_PULSE_US;
        pulse_end_us = SERVO_MAX_PULSE_US;
        angle_start_mdeg = SERVO_CENTER_ANGLE_MDEG;
        angle_end_mdeg = (int64_t)SERVO_MAX_ANGLE_DEG * 1000;
    }

    numerator = ((int64_t)pulse_us - pulse_start_us) *
                (angle_end_mdeg - angle_start_mdeg);
    denominator = pulse_end_us - pulse_start_us;
    angle_mdeg = angle_start_mdeg + ((numerator + (denominator / 2)) / denominator);
    if ((angle_mdeg < INT32_MIN) || (angle_mdeg > INT32_MAX))
    {
        return INT32_MIN;
    }
    return (int32_t)angle_mdeg;
}

ServoStatus Servo_ConvertAngleMilliDegToPulseUs(int32_t angle_mdeg, uint16_t *pulse_us)
{
    if ((pulse_us == NULL) || (angle_mdeg < SERVO_MIN_ANGLE_MDEG) ||
        (angle_mdeg > SERVO_MAX_ANGLE_MDEG))
    {
        return SERVO_STATUS_INVALID_ARGUMENT;
    }

    *pulse_us = Servo_AngleToPulse(angle_mdeg);
    return SERVO_STATUS_OK;
}

ServoStatus Servo_ConvertPulseUsToAngleMilliDeg(uint16_t pulse_us, int32_t *angle_mdeg)
{
    if ((angle_mdeg == NULL) || (pulse_us < SERVO_MIN_PULSE_US) ||
        (pulse_us > SERVO_MAX_PULSE_US))
    {
        return SERVO_STATUS_INVALID_ARGUMENT;
    }

    *angle_mdeg = Servo_PulseToAngle(pulse_us);
    return (*angle_mdeg == INT32_MIN) ? SERVO_STATUS_INVALID_ARGUMENT : SERVO_STATUS_OK;
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

    s_target_angle_mdeg = SERVO_CENTER_ANGLE_MDEG;
    s_commanded_angle_mdeg = SERVO_CENTER_ANGLE_MDEG;
    s_pulse_us = SERVO_CENTER_PULSE_US;
    s_angle_valid = true;
    s_enabled = true;
    s_initialized = true;
    return SERVO_STATUS_OK;
}

ServoStatus Servo_SetAngle(uint16_t angle_deg)
{
    return Servo_SetAngleMilliDeg((int32_t)angle_deg * 1000);
}

ServoStatus Servo_SetAngleMilliDeg(int32_t angle_mdeg)
{
    uint16_t pulse_us;
    ServoStatus conversion_status;

    if (!s_initialized)
    {
        return SERVO_STATUS_NOT_INITIALIZED;
    }
    conversion_status = Servo_ConvertAngleMilliDegToPulseUs(angle_mdeg, &pulse_us);
    if (conversion_status != SERVO_STATUS_OK)
    {
        return conversion_status;
    }

    __HAL_TIM_SET_COMPARE(&htim2, TIM_CHANNEL_1, pulse_us);
    s_target_angle_mdeg = angle_mdeg;
    s_commanded_angle_mdeg = Servo_PulseToAngle(pulse_us);
    s_pulse_us = pulse_us;
    s_angle_valid = true;
    return SERVO_STATUS_OK;
}

ServoStatus Servo_SetPulseUs(uint16_t pulse_us)
{
    int32_t angle_mdeg;

    if (!s_initialized)
    {
        return SERVO_STATUS_NOT_INITIALIZED;
    }
    if ((pulse_us >= (htim2.Init.Period + 1U)) ||
        (Servo_ConvertPulseUsToAngleMilliDeg(pulse_us, &angle_mdeg) != SERVO_STATUS_OK))
    {
        return SERVO_STATUS_INVALID_ARGUMENT;
    }

    __HAL_TIM_SET_COMPARE(&htim2, TIM_CHANNEL_1, pulse_us);
    s_pulse_us = pulse_us;
    s_angle_valid = false;
    return SERVO_STATUS_OK;
}

uint16_t Servo_GetAngle(void)
{
    return s_angle_valid ? (uint16_t)(s_target_angle_mdeg / 1000) : 0U;
}

int32_t Servo_GetTargetAngleMilliDeg(void)
{
    return s_angle_valid ? s_target_angle_mdeg : INT32_MIN;
}

int32_t Servo_GetCommandedAngleMilliDeg(void)
{
    return s_angle_valid ? s_commanded_angle_mdeg : INT32_MIN;
}

uint16_t Servo_GetPulseUs(void)
{
    return s_pulse_us;
}

bool Servo_IsAngleValid(void)
{
    return s_angle_valid;
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

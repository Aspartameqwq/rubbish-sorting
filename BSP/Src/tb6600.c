#include "tb6600.h"

#include "main.h"
#include "project_config.h"
#include "tb6600_timing.h"
#include "tim.h"

#include <limits.h>

#if (TB6600_ENABLE_ACTIVE_LEVEL != 0U) && (TB6600_ENABLE_ACTIVE_LEVEL != 1U)
#error "TB6600_ENABLE_ACTIVE_LEVEL must be 0 or 1"
#endif

#if (TB6600_DIR_FORWARD_LEVEL != 0U) && (TB6600_DIR_FORWARD_LEVEL != 1U)
#error "TB6600_DIR_FORWARD_LEVEL must be 0 or 1"
#endif

#if (TB6600_PULSE_ACTIVE_LEVEL != 0U) && (TB6600_PULSE_ACTIVE_LEVEL != 1U)
#error "TB6600_PULSE_ACTIVE_LEVEL must be 0 or 1"
#endif

#if (TB6600_STEP_FREQ_MAX_HZ == 0U)
#error "TB6600_STEP_FREQ_MAX_HZ must be greater than zero"
#endif

static volatile bool s_initialized;
static volatile bool s_enabled;
static volatile bool s_pulse_running;
static volatile bool s_stop_requested;
static volatile bool s_pulse_error;
static volatile uint32_t s_pulse_target;
static volatile uint32_t s_pulse_completed;
static uint32_t s_step_frequency_hz;

static GPIO_PinState TB6600_LevelFromActive(bool active, uint32_t active_level)
{
    const bool high = active ? (active_level != 0U) : (active_level == 0U);
    return high ? GPIO_PIN_SET : GPIO_PIN_RESET;
}

static TB6600Status TB6600_StopTimerFromInterrupt(void)
{
    if (HAL_TIM_PWM_Stop_IT(&htim3, TIM_CHANNEL_1) != HAL_OK)
    {
        s_pulse_error = true;
        return TB6600_STATUS_HAL_ERROR;
    }

    s_pulse_running = false;
    return TB6600_STATUS_OK;
}

TB6600Status TB6600_Init(void)
{
    TIM_OC_InitTypeDef output_compare = {0};

    if (s_initialized)
    {
        return TB6600_STATUS_OK;
    }

    if ((htim3.Instance != TIM3) ||
        (htim3.Init.Prescaler != TB6600_TIMER_PRESCALER) ||
        (htim3.Init.Period != 49999U) ||
        (htim3.Init.CounterMode != TIM_COUNTERMODE_UP) ||
        (TB6600_STEP_FREQ_MIN_HZ == 0U) ||
        (TB6600_STEP_FREQ_MAX_HZ < TB6600_STEP_FREQ_MIN_HZ) ||
        (TB6600_PULSE_HIGH_US == 0U) ||
        (TB6600_TIMER_TICK_HZ != 1000000U) ||
        (TB6600_PULSE_HIGH_US >= (TB6600_TIMER_TICK_HZ / TB6600_STEP_FREQ_MAX_HZ)) ||
        ((TB6600_DIR_GPIO_Port != GPIOB) || (TB6600_DIR_Pin != GPIO_PIN_12)) ||
        ((TB6600_ENA_GPIO_Port != GPIOB) || (TB6600_ENA_Pin != GPIO_PIN_13)))
    {
        return TB6600_STATUS_CONFIGURATION_ERROR;
    }

    output_compare.OCMode = TIM_OCMODE_PWM1;
    output_compare.Pulse = TB6600_PULSE_HIGH_US;
    output_compare.OCPolarity = (TB6600_PULSE_ACTIVE_LEVEL != 0U)
                                    ? TIM_OCPOLARITY_HIGH
                                    : TIM_OCPOLARITY_LOW;
    output_compare.OCFastMode = TIM_OCFAST_DISABLE;
    if (HAL_TIM_PWM_ConfigChannel(&htim3, &output_compare, TIM_CHANNEL_1) != HAL_OK)
    {
        return TB6600_STATUS_HAL_ERROR;
    }

    HAL_GPIO_WritePin(TB6600_ENA_GPIO_Port,
                      TB6600_ENA_Pin,
                      TB6600_LevelFromActive(false, TB6600_ENABLE_ACTIVE_LEVEL));
    HAL_GPIO_WritePin(TB6600_DIR_GPIO_Port,
                      TB6600_DIR_Pin,
                      TB6600_LevelFromActive(false, TB6600_DIR_FORWARD_LEVEL));
    __HAL_TIM_SET_COMPARE(&htim3, TIM_CHANNEL_1, TB6600_PULSE_HIGH_US);

    s_enabled = false;
    s_pulse_running = false;
    s_stop_requested = false;
    s_pulse_error = false;
    s_pulse_target = 0U;
    s_pulse_completed = 0U;
    s_step_frequency_hz = 0U;
    s_initialized = true;
    return TB6600_STATUS_OK;
}

TB6600Status TB6600_Enable(void)
{
    if (!s_initialized)
    {
        return TB6600_STATUS_NOT_INITIALIZED;
    }
    if (s_pulse_running)
    {
        return TB6600_STATUS_BUSY;
    }

    HAL_GPIO_WritePin(TB6600_ENA_GPIO_Port,
                      TB6600_ENA_Pin,
                      TB6600_LevelFromActive(true, TB6600_ENABLE_ACTIVE_LEVEL));
    s_enabled = true;
    return TB6600_STATUS_OK;
}

TB6600Status TB6600_Disable(void)
{
    if (!s_initialized)
    {
        return TB6600_STATUS_NOT_INITIALIZED;
    }
    if (s_pulse_running)
    {
        return TB6600_STATUS_BUSY;
    }

    HAL_GPIO_WritePin(TB6600_ENA_GPIO_Port,
                      TB6600_ENA_Pin,
                      TB6600_LevelFromActive(false, TB6600_ENABLE_ACTIVE_LEVEL));
    s_enabled = false;
    return TB6600_STATUS_OK;
}

TB6600Status TB6600_SetDirection(TB6600Direction direction)
{
    bool forward;
    const uint32_t output_level = TB6600_DIR_FORWARD_LEVEL;

    if (!s_initialized)
    {
        return TB6600_STATUS_NOT_INITIALIZED;
    }
    if ((direction != TB6600_DIRECTION_FORWARD) &&
        (direction != TB6600_DIRECTION_REVERSE))
    {
        return TB6600_STATUS_INVALID_ARGUMENT;
    }
    if (s_pulse_running)
    {
        return TB6600_STATUS_BUSY;
    }

    forward = (direction == TB6600_DIRECTION_FORWARD);
    HAL_GPIO_WritePin(TB6600_DIR_GPIO_Port,
                      TB6600_DIR_Pin,
                      TB6600_LevelFromActive(forward, output_level));
    return TB6600_STATUS_OK;
}

TB6600Status TB6600_SetStepFrequency(uint32_t frequency_hz)
{
    TB6600TimingStatus timing_status;
    uint16_t auto_reload;
    uint16_t pulse_compare;

    if (!s_initialized)
    {
        return TB6600_STATUS_NOT_INITIALIZED;
    }
    if ((frequency_hz < TB6600_STEP_FREQ_MIN_HZ) ||
        (frequency_hz > TB6600_STEP_FREQ_MAX_HZ))
    {
        return TB6600_STATUS_INVALID_ARGUMENT;
    }
    if (s_pulse_running)
    {
        return TB6600_STATUS_BUSY;
    }

    timing_status = TB6600Timing_Calculate(TB6600_TIMER_TICK_HZ,
                                           frequency_hz,
                                           TB6600_PULSE_HIGH_US,
                                           UINT16_MAX,
                                           &auto_reload,
                                           &pulse_compare);
    if (timing_status != TB6600_TIMING_STATUS_OK)
    {
        return TB6600_STATUS_CONFIGURATION_ERROR;
    }

    __HAL_TIM_SET_AUTORELOAD(&htim3, auto_reload);
    __HAL_TIM_SET_COMPARE(&htim3, TIM_CHANNEL_1, pulse_compare);
    __HAL_TIM_SET_COUNTER(&htim3, 0U);
    s_step_frequency_hz = frequency_hz;
    return TB6600_STATUS_OK;
}

TB6600Status TB6600_StartPulse(uint32_t pulse_count)
{
    if (!s_initialized)
    {
        return TB6600_STATUS_NOT_INITIALIZED;
    }
    if ((pulse_count == 0U) || (s_step_frequency_hz == 0U))
    {
        return TB6600_STATUS_INVALID_ARGUMENT;
    }
    if (!s_enabled)
    {
        return TB6600_STATUS_CONFIGURATION_ERROR;
    }
    if (s_pulse_running)
    {
        return TB6600_STATUS_BUSY;
    }

    s_pulse_target = pulse_count;
    s_pulse_completed = 0U;
    s_stop_requested = false;
    s_pulse_error = false;
    __HAL_TIM_SET_COUNTER(&htim3, 0U);
    __HAL_TIM_CLEAR_FLAG(&htim3, TIM_FLAG_UPDATE | TIM_FLAG_CC1);
    s_pulse_running = true;

    if (HAL_TIM_PWM_Start_IT(&htim3, TIM_CHANNEL_1) != HAL_OK)
    {
        if (HAL_TIM_PWM_Stop_IT(&htim3, TIM_CHANNEL_1) != HAL_OK)
        {
            s_pulse_error = true;
        }
        s_pulse_running = false;
        s_pulse_target = 0U;
        return TB6600_STATUS_HAL_ERROR;
    }
    return TB6600_STATUS_OK;
}

TB6600Status TB6600_RequestPulseStop(void)
{
    if (!s_initialized)
    {
        return TB6600_STATUS_NOT_INITIALIZED;
    }
    if (s_pulse_running)
    {
        s_stop_requested = true;
    }
    return TB6600_STATUS_OK;
}

TB6600Status TB6600_StopPulse(void)
{
    if (!s_initialized)
    {
        return TB6600_STATUS_NOT_INITIALIZED;
    }
    if (!s_pulse_running)
    {
        return TB6600_STATUS_OK;
    }

    if (HAL_TIM_PWM_Stop_IT(&htim3, TIM_CHANNEL_1) != HAL_OK)
    {
        /* The compare IRQ may have stopped the channel just before this call. */
        if (!s_pulse_running)
        {
            s_stop_requested = false;
            return TB6600_STATUS_OK;
        }
        s_pulse_error = true;
        return TB6600_STATUS_HAL_ERROR;
    }
    s_pulse_running = false;
    s_stop_requested = false;
    return TB6600_STATUS_OK;
}

bool TB6600_IsInitialized(void)
{
    return s_initialized;
}

bool TB6600_IsEnabled(void)
{
    return s_enabled;
}

bool TB6600_IsPulseRunning(void)
{
    return s_pulse_running;
}

bool TB6600_HasPulseError(void)
{
    return s_pulse_error;
}

uint32_t TB6600_GetStepFrequency(void)
{
    return s_step_frequency_hz;
}

uint32_t TB6600_GetCompletedPulseCount(void)
{
    return s_pulse_completed;
}

uint32_t TB6600_GetTickMs(void)
{
    return HAL_GetTick();
}

void HAL_TIM_PWM_PulseFinishedCallback(TIM_HandleTypeDef *htim)
{
    if ((htim == &htim3) && s_pulse_running)
    {
        if (s_pulse_completed < s_pulse_target)
        {
            s_pulse_completed++;
        }

        if ((s_pulse_completed >= s_pulse_target) || s_stop_requested)
        {
            (void)TB6600_StopTimerFromInterrupt();
            s_stop_requested = false;
        }
    }
}

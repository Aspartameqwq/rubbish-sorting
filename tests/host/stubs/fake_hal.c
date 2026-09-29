#include "test_fakes.h"

#include "hc04.h"
#include "main.h"
#include "tim.h"

#include <stdbool.h>
#include <string.h>

#define TEST_UART_BUFFER_CAPACITY 4096U

GPIO_TypeDef test_gpio_a;
GPIO_TypeDef test_gpio_b;
TIM_TypeDef test_tim2_instance;
TIM_TypeDef test_tim3_instance;
TIM_HandleTypeDef htim2 = {&test_tim2_instance, {71U, 19999U, TIM_COUNTERMODE_UP}, 0U, 19999U, 0U};
TIM_HandleTypeDef htim3 = {&test_tim3_instance, {71U, 49999U, TIM_COUNTERMODE_UP}, 10U, 49999U, 0U};

static uint8_t s_uart_rx[TEST_UART_BUFFER_CAPACITY];
static size_t s_uart_rx_read;
static size_t s_uart_rx_write;
static char s_uart_tx[TEST_UART_BUFFER_CAPACITY];
static size_t s_uart_tx_length;
static uint32_t s_tick_ms;
static uint32_t s_pwm_start_count;
static uint32_t s_pwm_stop_count;
static bool s_pwm_active;
static bool s_fail_next_pwm_start;
static uint32_t s_gpio_a_state;
static uint32_t s_gpio_b_state;
static bool s_tim3_pwm_configured;
static uint32_t s_tim3_pwm_polarity;

void HAL_GPIO_WritePin(GPIO_TypeDef *port, uint16_t pin, GPIO_PinState state)
{
    uint32_t *port_state = NULL;

    if (port == GPIOA)
    {
        port_state = &s_gpio_a_state;
    }
    else if (port == GPIOB)
    {
        port_state = &s_gpio_b_state;
    }

    if (port_state != NULL)
    {
        if (state == GPIO_PIN_SET)
        {
            *port_state |= pin;
        }
        else
        {
            *port_state &= ~((uint32_t)pin);
        }
    }
}

uint32_t HAL_GetTick(void)
{
    return s_tick_ms;
}

uint32_t SystemTime_GetMs(void)
{
    return HAL_GetTick();
}

void TestFakes_SetTick(uint32_t tick_ms)
{
    s_tick_ms = tick_ms;
}

uint32_t TestFakes_GetTick(void)
{
    return s_tick_ms;
}

void TestTim_SetCompare(TIM_HandleTypeDef *timer, uint32_t channel, uint32_t value)
{
    (void)channel;
    timer->compare1 = value;
}

void TestTim_SetAutoReload(TIM_HandleTypeDef *timer, uint32_t value)
{
    timer->autoreload = value;
    timer->Init.Period = value;
}

void TestTim_SetCounter(TIM_HandleTypeDef *timer, uint32_t value)
{
    timer->counter = value;
}

void TestTim_ClearFlag(TIM_HandleTypeDef *timer, uint32_t flags)
{
    (void)timer;
    (void)flags;
}

HAL_StatusTypeDef HAL_TIM_PWM_Start(TIM_HandleTypeDef *timer, uint32_t channel)
{
    (void)timer;
    (void)channel;
    return HAL_OK;
}

HAL_StatusTypeDef HAL_TIM_PWM_Stop(TIM_HandleTypeDef *timer, uint32_t channel)
{
    (void)timer;
    (void)channel;
    return HAL_OK;
}

HAL_StatusTypeDef HAL_TIM_PWM_Start_IT(TIM_HandleTypeDef *timer, uint32_t channel)
{
    (void)timer;
    (void)channel;
    s_pwm_start_count++;
    if (s_fail_next_pwm_start)
    {
        s_fail_next_pwm_start = false;
        return HAL_ERROR;
    }
    s_pwm_active = true;
    return HAL_OK;
}

HAL_StatusTypeDef HAL_TIM_PWM_Stop_IT(TIM_HandleTypeDef *timer, uint32_t channel)
{
    (void)timer;
    (void)channel;
    s_pwm_stop_count++;
    s_pwm_active = false;
    return HAL_OK;
}

HAL_StatusTypeDef HAL_TIM_PWM_ConfigChannel(TIM_HandleTypeDef *timer,
                                             TIM_OC_InitTypeDef *config,
                                             uint32_t channel)
{
    timer->compare1 = config->Pulse;
    if ((timer == &htim3) && (channel == TIM_CHANNEL_1))
    {
        s_tim3_pwm_configured = true;
        s_tim3_pwm_polarity = config->OCPolarity;
    }
    return HAL_OK;
}

HC04Status HC04_Init(void)
{
    return HC04_STATUS_OK;
}

void HC04_Process(void)
{
}

HC04Status HC04_ReadByte(uint8_t *byte)
{
    if ((byte == NULL) || (s_uart_rx_read == s_uart_rx_write))
    {
        return HC04_STATUS_NO_DATA;
    }
    *byte = s_uart_rx[s_uart_rx_read % TEST_UART_BUFFER_CAPACITY];
    s_uart_rx_read++;
    return HC04_STATUS_OK;
}

HC04Status HC04_Send(const uint8_t *data, uint16_t length)
{
    if ((data == NULL) || (length > (TEST_UART_BUFFER_CAPACITY - s_uart_tx_length - 1U)))
    {
        return HC04_STATUS_INVALID_ARGUMENT;
    }
    (void)memcpy(&s_uart_tx[s_uart_tx_length], data, length);
    s_uart_tx_length += length;
    s_uart_tx[s_uart_tx_length] = '\0';
    return HC04_STATUS_OK;
}

void TestFakes_ResetUart(void)
{
    s_uart_rx_read = 0U;
    s_uart_rx_write = 0U;
    s_uart_tx_length = 0U;
    s_uart_tx[0] = '\0';
}

void TestFakes_FeedUart(const char *text)
{
    while ((text != NULL) && (*text != '\0') &&
           ((s_uart_rx_write - s_uart_rx_read) < TEST_UART_BUFFER_CAPACITY))
    {
        s_uart_rx[s_uart_rx_write % TEST_UART_BUFFER_CAPACITY] = (uint8_t)*text;
        s_uart_rx_write++;
        text++;
    }
}

size_t TestFakes_UartPending(void)
{
    return s_uart_rx_write - s_uart_rx_read;
}

const char *TestFakes_TxData(void)
{
    return s_uart_tx;
}

void TestFakes_ClearTx(void)
{
    s_uart_tx_length = 0U;
    s_uart_tx[0] = '\0';
}

uint32_t TestFakes_GetPwmStartCount(void)
{
    return s_pwm_start_count;
}

uint32_t TestFakes_GetPwmStopCount(void)
{
    return s_pwm_stop_count;
}

void TestFakes_FailNextPwmStart(void)
{
    s_fail_next_pwm_start = true;
}

GPIO_PinState TestFakes_GetGpioState(GPIO_TypeDef *port, uint16_t pin)
{
    uint32_t port_state;

    if (port == GPIOA)
    {
        port_state = s_gpio_a_state;
    }
    else if (port == GPIOB)
    {
        port_state = s_gpio_b_state;
    }
    else
    {
        return GPIO_PIN_RESET;
    }

    return ((port_state & pin) != 0U) ? GPIO_PIN_SET : GPIO_PIN_RESET;
}

bool TestFakes_IsTim3PwmConfigured(void)
{
    return s_tim3_pwm_configured;
}

uint32_t TestFakes_GetTim3PwmPolarity(void)
{
    return s_tim3_pwm_polarity;
}

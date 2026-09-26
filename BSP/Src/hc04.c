#include "hc04.h"

#include "project_config.h"
#include "usart.h"

#include <stdbool.h>

#if (HC04_RX_DMA_BUFFER_SIZE < 2U) || ((HC04_RX_DMA_BUFFER_SIZE % 2U) != 0U)
#error "HC-04 DMA buffer size must be an even number greater than one"
#endif

static uint8_t s_rx_dma_buffer[HC04_RX_DMA_BUFFER_SIZE];

static volatile uint32_t s_dma_half_event_count;
static volatile uint32_t s_rx_error_count;
static volatile uint8_t s_rx_error_pending;
static volatile uint8_t s_rx_restarting;

static uint32_t s_dma_base_total;
static uint32_t s_produced_total;
static uint32_t s_consumer_total;
static uint32_t s_rx_overflow_count;
static uint32_t s_last_restart_tick;
static bool s_initialized;
static bool s_rx_active;
static bool s_rx_restart_pending;
static bool s_protocol_resync_pending;

static uint32_t HC04_SnapshotProducedTotal(uint8_t *error_pending)
{
    uint32_t primask;
    uint32_t half_event_count;
    uint32_t remaining;
    uint32_t dma_position;
    uint32_t produced_total;

    primask = __get_PRIMASK();
    __disable_irq();

    half_event_count = s_dma_half_event_count;
    remaining = __HAL_DMA_GET_COUNTER(huart1.hdmarx);
    if (error_pending != NULL)
    {
        *error_pending = s_rx_error_pending;
        s_rx_error_pending = 0U;
    }

    __set_PRIMASK(primask);

    if (remaining > HC04_RX_DMA_BUFFER_SIZE)
    {
        remaining = HC04_RX_DMA_BUFFER_SIZE;
    }

    dma_position = HC04_RX_DMA_BUFFER_SIZE - remaining;
    if (((half_event_count & 1U) != 0U) &&
        (dma_position < (HC04_RX_DMA_BUFFER_SIZE / 2U)))
    {
        /* Account for a wrapped DMA TC whose IRQ became pending during the snapshot. */
        half_event_count++;
    }

    produced_total = s_dma_base_total +
                     ((half_event_count / 2U) * HC04_RX_DMA_BUFFER_SIZE) +
                     dma_position;
    return produced_total;
}

static HAL_StatusTypeDef HC04_StartRx(void)
{
    return HAL_UARTEx_ReceiveToIdle_DMA(&huart1,
                                        s_rx_dma_buffer,
                                        (uint16_t)HC04_RX_DMA_BUFFER_SIZE);
}

static HAL_StatusTypeDef HC04_RestartRx(void)
{
    HAL_StatusTypeDef abort_status;
    HAL_StatusTypeDef start_status;
    uint32_t primask;

    s_rx_restarting = 1U;
    abort_status = HAL_UART_AbortReceive(&huart1);
    if (abort_status != HAL_OK)
    {
        s_rx_active = false;
        s_last_restart_tick = HAL_GetTick();
        s_rx_restarting = 0U;
        return abort_status;
    }

    s_dma_base_total = s_produced_total;
    s_consumer_total = s_produced_total;
    primask = __get_PRIMASK();
    __disable_irq();
    s_dma_half_event_count = 0U;
    s_rx_error_pending = 0U;
    __set_PRIMASK(primask);

    start_status = HC04_StartRx();
    s_rx_active = (start_status == HAL_OK);
    s_rx_restart_pending = !s_rx_active;
    s_last_restart_tick = HAL_GetTick();
    s_rx_restarting = 0U;
    return start_status;
}

HC04Status HC04_Init(void)
{
    HAL_StatusTypeDef hal_status;

    if (s_initialized)
    {
        return s_rx_active ? HC04_STATUS_OK : HC04_STATUS_HAL_ERROR;
    }
    if ((huart1.Instance != USART1) || (huart1.hdmarx == NULL))
    {
        return HC04_STATUS_HAL_ERROR;
    }

    if (huart1.Init.BaudRate != HC04_BAUDRATE)
    {
        huart1.Init.BaudRate = HC04_BAUDRATE;
        if (HAL_UART_Init(&huart1) != HAL_OK)
        {
            return HC04_STATUS_HAL_ERROR;
        }
    }

    s_dma_half_event_count = 0U;
    s_rx_error_count = 0U;
    s_rx_error_pending = 0U;
    s_rx_restarting = 0U;
    s_dma_base_total = 0U;
    s_produced_total = 0U;
    s_consumer_total = 0U;
    s_rx_overflow_count = 0U;
    s_last_restart_tick = HAL_GetTick();
    s_rx_restart_pending = false;
    s_protocol_resync_pending = false;
    s_initialized = true;

    hal_status = HC04_StartRx();
    s_rx_active = (hal_status == HAL_OK);
    s_rx_restart_pending = !s_rx_active;
    return s_rx_active ? HC04_STATUS_OK : HC04_STATUS_HAL_ERROR;
}

void HC04_Process(void)
{
    uint8_t error_pending = 0U;
    uint32_t now;

    if (!s_initialized)
    {
        return;
    }

    if (s_rx_active)
    {
        s_produced_total = HC04_SnapshotProducedTotal(&error_pending);
        if ((uint32_t)(s_produced_total - s_consumer_total) >= HC04_RX_DMA_BUFFER_SIZE)
        {
            s_rx_overflow_count++;
            s_consumer_total = s_produced_total;
            s_protocol_resync_pending = true;
        }
    }
    else
    {
        (void)HC04_SnapshotProducedTotal(&error_pending);
    }

    if (error_pending != 0U)
    {
        s_protocol_resync_pending = true;
        s_consumer_total = s_produced_total;
        s_rx_restart_pending = true;
    }

    now = HAL_GetTick();
    if (s_rx_restart_pending && ((uint32_t)(now - s_last_restart_tick) >= HC04_RX_RESTART_RETRY_MS))
    {
        (void)HC04_RestartRx();
    }
}

HC04Status HC04_ReadByte(uint8_t *byte)
{
    if (byte == NULL)
    {
        return HC04_STATUS_INVALID_ARGUMENT;
    }
    if (!s_initialized)
    {
        return HC04_STATUS_NOT_INITIALIZED;
    }
    if (s_protocol_resync_pending)
    {
        s_protocol_resync_pending = false;
        return HC04_STATUS_OVERFLOW;
    }
    if (s_consumer_total == s_produced_total)
    {
        return HC04_STATUS_NO_DATA;
    }

    *byte = s_rx_dma_buffer[s_consumer_total % HC04_RX_DMA_BUFFER_SIZE];
    s_consumer_total++;
    return HC04_STATUS_OK;
}

HC04Status HC04_Send(const uint8_t *data, uint16_t length)
{
    if ((data == NULL) && (length != 0U))
    {
        return HC04_STATUS_INVALID_ARGUMENT;
    }
    if (!s_initialized)
    {
        return HC04_STATUS_NOT_INITIALIZED;
    }
    if (length == 0U)
    {
        return HC04_STATUS_OK;
    }

    /* HAL's transmit API takes a mutable pointer but does not modify TX bytes. */
    if (HAL_UART_Transmit(&huart1, (uint8_t *)data, length, HC04_TX_TIMEOUT_MS) != HAL_OK)
    {
        return HC04_STATUS_HAL_ERROR;
    }
    return HC04_STATUS_OK;
}

uint32_t HC04_GetRxOverflowCount(void)
{
    return s_rx_overflow_count;
}

uint32_t HC04_GetRxErrorCount(void)
{
    return s_rx_error_count;
}

void HAL_UARTEx_RxEventCallback(UART_HandleTypeDef *huart, uint16_t size)
{
    HAL_UART_RxEventTypeTypeDef event_type;

    (void)size;
    if ((huart != &huart1) || (s_rx_restarting != 0U))
    {
        return;
    }

    event_type = HAL_UARTEx_GetRxEventType(huart);
    if ((event_type == HAL_UART_RXEVENT_HT) || (event_type == HAL_UART_RXEVENT_TC))
    {
        s_dma_half_event_count++;
    }
}

void HAL_UART_ErrorCallback(UART_HandleTypeDef *huart)
{
    if ((huart != &huart1) || (s_rx_restarting != 0U))
    {
        return;
    }

    s_rx_error_count++;
    s_rx_error_pending = 1U;
}

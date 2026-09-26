#include "protocol.h"

#include "hc04.h"
#include "project_config.h"
#include "servo.h"

#include <stdbool.h>
#include <stdint.h>
#include <string.h>

static uint8_t s_command_line[PROTOCOL_COMMAND_BUFFER_SIZE];
static uint16_t s_command_length;
static bool s_discard_until_lf;
static bool s_reply_error_after_resync;
static uint32_t s_tx_error_count;

static void Protocol_Send(const uint8_t *data, uint16_t length)
{
    if (HC04_Send(data, length) != HC04_STATUS_OK)
    {
        s_tx_error_count++;
    }
}

static void Protocol_SendError(void)
{
    static const uint8_t response[] = "ERR\r\n";
    Protocol_Send(response, (uint16_t)(sizeof(response) - 1U));
}

static void Protocol_SendOk(void)
{
    static const uint8_t response[] = "OK\r\n";
    Protocol_Send(response, (uint16_t)(sizeof(response) - 1U));
}

static bool Protocol_ParseDecimal(uint16_t prefix_length, uint16_t *value)
{
    uint16_t index = prefix_length;
    uint32_t parsed_value = 0U;
    bool has_digit = false;

    while ((index < s_command_length) && (s_command_line[index] == (uint8_t)' '))
    {
        index++;
    }

    while (index < s_command_length)
    {
        const uint8_t character = s_command_line[index];
        uint32_t digit;

        if ((character < (uint8_t)'0') || (character > (uint8_t)'9'))
        {
            break;
        }

        has_digit = true;
        digit = (uint32_t)(character - (uint8_t)'0');
        if (parsed_value > ((UINT32_MAX - digit) / 10U))
        {
            return false;
        }
        parsed_value = (parsed_value * 10U) + digit;
        index++;
    }

    if (!has_digit || (parsed_value > UINT16_MAX))
    {
        return false;
    }

    while ((index < s_command_length) && (s_command_line[index] == (uint8_t)' '))
    {
        index++;
    }
    if (index != s_command_length)
    {
        return false;
    }

    *value = (uint16_t)parsed_value;
    return true;
}

static uint16_t Protocol_AppendDecimal(uint16_t value, uint8_t *output)
{
    uint8_t reverse_digits[5];
    uint16_t digit_count = 0U;
    uint16_t output_count = 0U;

    do
    {
        reverse_digits[digit_count] = (uint8_t)('0' + (value % 10U));
        digit_count++;
        value = (uint16_t)(value / 10U);
    } while (value != 0U);

    while (digit_count > 0U)
    {
        digit_count--;
        output[output_count] = reverse_digits[digit_count];
        output_count++;
    }
    return output_count;
}

static void Protocol_SendServoAngle(void)
{
    static const uint8_t prefix[] = "SERVO ";
    uint8_t response[16];
    uint16_t length = (uint16_t)(sizeof(prefix) - 1U);

    (void)memcpy(response, prefix, sizeof(prefix) - 1U);
    length = (uint16_t)(length + Protocol_AppendDecimal(Servo_GetAngle(), &response[length]));
    response[length] = (uint8_t)'\r';
    length++;
    response[length] = (uint8_t)'\n';
    length++;
    Protocol_Send(response, length);
}

static bool Protocol_ExecuteCommand(void)
{
    static const uint8_t ping_command[] = "PING";
    static const uint8_t servo_query[] = "SERVO?";
    static const uint8_t servo_prefix[] = "SERVO ";
    static const uint8_t servo_us_prefix[] = "SERVO_US ";
    uint16_t value;

    if ((s_command_length == (sizeof(ping_command) - 1U)) &&
        (memcmp(s_command_line, ping_command, sizeof(ping_command) - 1U) == 0))
    {
        static const uint8_t response[] = "PONG\r\n";
        Protocol_Send(response, (uint16_t)(sizeof(response) - 1U));
        return true;
    }

    if ((s_command_length == (sizeof(servo_query) - 1U)) &&
        (memcmp(s_command_line, servo_query, sizeof(servo_query) - 1U) == 0))
    {
        if (Servo_IsInitialized())
        {
            Protocol_SendServoAngle();
        }
        else
        {
            Protocol_SendError();
        }
        return true;
    }

    if ((s_command_length > (sizeof(servo_prefix) - 1U)) &&
        (memcmp(s_command_line, servo_prefix, sizeof(servo_prefix) - 1U) == 0))
    {
        if (Protocol_ParseDecimal((uint16_t)(sizeof(servo_prefix) - 1U), &value) &&
            (Servo_SetAngle(value) == SERVO_STATUS_OK))
        {
            Protocol_SendOk();
            return true;
        }
        return false;
    }

    if ((s_command_length > (sizeof(servo_us_prefix) - 1U)) &&
        (memcmp(s_command_line, servo_us_prefix, sizeof(servo_us_prefix) - 1U) == 0))
    {
        if (Protocol_ParseDecimal((uint16_t)(sizeof(servo_us_prefix) - 1U), &value) &&
            (Servo_SetPulseUs(value) == SERVO_STATUS_OK))
        {
            Protocol_SendOk();
            return true;
        }
        return false;
    }

    return false;
}

static void Protocol_ProcessByte(uint8_t byte)
{
    if (s_discard_until_lf)
    {
        if (byte == (uint8_t)'\n')
        {
            s_discard_until_lf = false;
            s_command_length = 0U;
            if (s_reply_error_after_resync)
            {
                Protocol_SendError();
                s_reply_error_after_resync = false;
            }
        }
        return;
    }

    if (byte == (uint8_t)'\r')
    {
        return;
    }
    if (byte == (uint8_t)'\n')
    {
        if (s_command_length != 0U)
        {
            s_command_line[s_command_length] = 0U;
            if (!Protocol_ExecuteCommand())
            {
                Protocol_SendError();
            }
            s_command_length = 0U;
        }
        return;
    }

    if (s_command_length >= (PROTOCOL_COMMAND_BUFFER_SIZE - 1U))
    {
        s_command_length = 0U;
        s_discard_until_lf = true;
        s_reply_error_after_resync = true;
        return;
    }

    s_command_line[s_command_length] = byte;
    s_command_length++;
}

void Protocol_Init(void)
{
    s_command_length = 0U;
    s_discard_until_lf = false;
    s_reply_error_after_resync = false;
    s_tx_error_count = 0U;
}

void Protocol_Process(void)
{
    uint8_t byte;
    HC04Status status;

    for (;;)
    {
        status = HC04_ReadByte(&byte);
        if (status == HC04_STATUS_NO_DATA)
        {
            break;
        }
        if (status == HC04_STATUS_OVERFLOW)
        {
            s_command_length = 0U;
            s_discard_until_lf = true;
            s_reply_error_after_resync = true;
            continue;
        }
        if (status != HC04_STATUS_OK)
        {
            break;
        }

        Protocol_ProcessByte(byte);
    }
}

uint32_t Protocol_GetTxErrorCount(void)
{
    return s_tx_error_count;
}

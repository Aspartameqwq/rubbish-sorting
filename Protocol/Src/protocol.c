#include "protocol.h"

#include "hc04.h"
#include "control_debug_config.h"
#include "pitch_axis.h"
#include "project_config.h"
#include "servo.h"
#include "stepper.h"

#include <stdbool.h>
#include <stddef.h>
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

#if (RAW_BENCH_COMMANDS_ENABLE == 1)
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
#endif

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

static uint16_t Protocol_AppendUnsigned32(uint32_t value, uint8_t *output)
{
    uint8_t reverse_digits[10];
    uint16_t digit_count = 0U;
    uint16_t output_count = 0U;

    do
    {
        reverse_digits[digit_count] = (uint8_t)('0' + (value % 10U));
        digit_count++;
        value /= 10U;
    } while (value != 0U);

    while (digit_count > 0U)
    {
        digit_count--;
        output[output_count] = reverse_digits[digit_count];
        output_count++;
    }
    return output_count;
}

static uint16_t Protocol_AppendSigned32(int32_t value, uint8_t *output)
{
    uint16_t length = 0U;
    uint32_t magnitude;

    if (value < 0)
    {
        output[length] = (uint8_t)'-';
        length++;
        magnitude = (uint32_t)(-(int64_t)value);
    }
    else
    {
        magnitude = (uint32_t)value;
    }
    return (uint16_t)(length + Protocol_AppendUnsigned32(magnitude, &output[length]));
}

#if (RAW_BENCH_COMMANDS_ENABLE == 1)
static bool Protocol_ParseSigned32AndUnsigned32(uint16_t prefix_length,
                                                int32_t *signed_value,
                                                uint32_t *unsigned_value)
{
    uint16_t index = prefix_length;
    uint32_t magnitude = 0U;
    uint32_t parsed_unsigned = 0U;
    uint32_t magnitude_limit;
    bool negative = false;
    bool has_digit = false;

    while ((index < s_command_length) && (s_command_line[index] == (uint8_t)' '))
    {
        index++;
    }
    if (index < s_command_length)
    {
        if (s_command_line[index] == (uint8_t)'-')
        {
            negative = true;
            index++;
        }
        else if (s_command_line[index] == (uint8_t)'+')
        {
            index++;
        }
    }

    magnitude_limit = negative ? ((uint32_t)INT32_MAX + 1U) : (uint32_t)INT32_MAX;
    while (index < s_command_length)
    {
        const uint8_t character = s_command_line[index];
        uint32_t digit;

        if ((character < (uint8_t)'0') || (character > (uint8_t)'9'))
        {
            break;
        }
        digit = (uint32_t)(character - (uint8_t)'0');
        if (magnitude > ((magnitude_limit - digit) / 10U))
        {
            return false;
        }
        magnitude = (magnitude * 10U) + digit;
        has_digit = true;
        index++;
    }
    if (!has_digit)
    {
        return false;
    }

    if ((index >= s_command_length) || (s_command_line[index] != (uint8_t)' '))
    {
        return false;
    }
    while ((index < s_command_length) && (s_command_line[index] == (uint8_t)' '))
    {
        index++;
    }

    has_digit = false;
    while (index < s_command_length)
    {
        const uint8_t character = s_command_line[index];
        uint32_t digit;

        if ((character < (uint8_t)'0') || (character > (uint8_t)'9'))
        {
            break;
        }
        digit = (uint32_t)(character - (uint8_t)'0');
        if (parsed_unsigned > ((UINT32_MAX - digit) / 10U))
        {
            return false;
        }
        parsed_unsigned = (parsed_unsigned * 10U) + digit;
        has_digit = true;
        index++;
    }
    if (!has_digit)
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

    if (negative)
    {
        *signed_value = (magnitude == ((uint32_t)INT32_MAX + 1U))
                            ? INT32_MIN
                            : -(int32_t)magnitude;
    }
    else
    {
        *signed_value = (int32_t)magnitude;
    }
    *unsigned_value = parsed_unsigned;
    return true;
}
#endif

static bool Protocol_ParseSignedDecimal32(uint16_t prefix_length, int32_t *value)
{
    uint16_t index = prefix_length;
    uint32_t magnitude = 0U;
    uint32_t magnitude_limit;
    bool negative = false;
    bool has_digit = false;

    if (value == NULL)
    {
        return false;
    }
    while ((index < s_command_length) && (s_command_line[index] == (uint8_t)' '))
    {
        index++;
    }
    if (index < s_command_length)
    {
        if (s_command_line[index] == (uint8_t)'-')
        {
            negative = true;
            index++;
        }
        else if (s_command_line[index] == (uint8_t)'+')
        {
            index++;
        }
    }

    magnitude_limit = negative ? ((uint32_t)INT32_MAX + 1U) : (uint32_t)INT32_MAX;
    while (index < s_command_length)
    {
        const uint8_t character = s_command_line[index];
        uint32_t digit;

        if ((character < (uint8_t)'0') || (character > (uint8_t)'9'))
        {
            break;
        }
        digit = (uint32_t)(character - (uint8_t)'0');
        if (magnitude > ((magnitude_limit - digit) / 10U))
        {
            return false;
        }
        magnitude = (magnitude * 10U) + digit;
        has_digit = true;
        index++;
    }
    if (!has_digit)
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

    if (negative)
    {
        *value = (magnitude == ((uint32_t)INT32_MAX + 1U))
                     ? INT32_MIN
                     : -(int32_t)magnitude;
    }
    else
    {
        *value = (int32_t)magnitude;
    }
    return true;
}

static void Protocol_SendStepperStatus(void)
{
    static const uint8_t prefix[] = "STEPPER ";
    static const uint8_t position_label[] = " POS=";
    static const uint8_t remaining_label[] = " REM=";
    static const uint8_t frequency_label[] = " FREQ=";
    static const uint8_t *const state_names[] = {
        (const uint8_t *)"UNINITIALIZED",
        (const uint8_t *)"DISABLED",
        (const uint8_t *)"IDLE",
        (const uint8_t *)"DIR_SETUP",
        (const uint8_t *)"RUNNING",
        (const uint8_t *)"STOPPING",
        (const uint8_t *)"FAULT"};
    static const uint8_t state_lengths[] = {13U, 8U, 4U, 9U, 7U, 8U, 5U};
    uint8_t response[80];
    uint16_t length = 0U;
    StepperState state = Stepper_GetState();

    if ((uint32_t)state >= (sizeof(state_names) / sizeof(state_names[0])))
    {
        state = STEPPER_STATE_FAULT;
    }

    (void)memcpy(&response[length], prefix, sizeof(prefix) - 1U);
    length = (uint16_t)(length + sizeof(prefix) - 1U);
    (void)memcpy(&response[length], state_names[state], state_lengths[state]);
    length = (uint16_t)(length + state_lengths[state]);
    (void)memcpy(&response[length], position_label, sizeof(position_label) - 1U);
    length = (uint16_t)(length + sizeof(position_label) - 1U);
    length = (uint16_t)(length + Protocol_AppendSigned32(Stepper_GetCommandedPosition(),
                                                         &response[length]));
    (void)memcpy(&response[length], remaining_label, sizeof(remaining_label) - 1U);
    length = (uint16_t)(length + sizeof(remaining_label) - 1U);
    length = (uint16_t)(length + Protocol_AppendUnsigned32(Stepper_GetRemainingSteps(),
                                                           &response[length]));
    (void)memcpy(&response[length], frequency_label, sizeof(frequency_label) - 1U);
    length = (uint16_t)(length + sizeof(frequency_label) - 1U);
    length = (uint16_t)(length + Protocol_AppendUnsigned32(Stepper_GetStepFrequency(),
                                                           &response[length]));
    response[length] = (uint8_t)'\r';
    length++;
    response[length] = (uint8_t)'\n';
    length++;
    Protocol_Send(response, length);
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

static void Protocol_SendServoRawPulse(void)
{
    static const uint8_t prefix[] = "SERVO RAW ";
    uint8_t response[20];
    uint16_t length = (uint16_t)(sizeof(prefix) - 1U);

    (void)memcpy(response, prefix, sizeof(prefix) - 1U);
    length = (uint16_t)(length + Protocol_AppendDecimal(Servo_GetPulseUs(), &response[length]));
    response[length] = (uint8_t)'\r';
    length++;
    response[length] = (uint8_t)'\n';
    length++;
    Protocol_Send(response, length);
}

static bool Protocol_ExecuteCommand(void)
{
    static const uint8_t ping_command[] = "PING";
    static const uint8_t pitch_prefix[] = "PITCH ";
    static const uint8_t servo_query[] = "SERVO?";
    static const uint8_t servo_prefix[] = "SERVO ";
    static const uint8_t servo_us_prefix[] = "SERVO_US ";
    static const uint8_t stepper_enable[] = "STEPPER ENABLE";
    static const uint8_t stepper_disable[] = "STEPPER DISABLE";
    static const uint8_t stepper_move_prefix[] = "STEPPER MOVE ";
    static const uint8_t stepper_stop[] = "STEPPER STOP";
    static const uint8_t stepper_query[] = "STEPPER?";
    int32_t pitch_target_mdeg;
#if (RAW_BENCH_COMMANDS_ENABLE == 1)
    uint16_t value;
#endif

    if ((s_command_length == (sizeof(ping_command) - 1U)) &&
        (memcmp(s_command_line, ping_command, sizeof(ping_command) - 1U) == 0))
    {
        static const uint8_t response[] = "PONG\r\n";
        Protocol_Send(response, (uint16_t)(sizeof(response) - 1U));
        return true;
    }

    if ((s_command_length > (sizeof(pitch_prefix) - 1U)) &&
        (memcmp(s_command_line, pitch_prefix, sizeof(pitch_prefix) - 1U) == 0))
    {
        if (Protocol_ParseSignedDecimal32((uint16_t)(sizeof(pitch_prefix) - 1U),
                                          &pitch_target_mdeg) &&
            (PitchAxis_SetTargetMilliDeg(pitch_target_mdeg) == PITCH_AXIS_STATUS_OK))
        {
            Protocol_SendOk();
        }
        else
        {
            Protocol_SendError();
        }
        return true;
    }

    if ((s_command_length == (sizeof(stepper_enable) - 1U)) &&
        (memcmp(s_command_line, stepper_enable, sizeof(stepper_enable) - 1U) == 0))
    {
#if (RAW_BENCH_COMMANDS_ENABLE == 1)
        if (Stepper_Enable() == STEPPER_STATUS_OK)
        {
            Protocol_SendOk();
        }
        else
        {
            Protocol_SendError();
        }
#else
        Protocol_SendError();
#endif
        return true;
    }

    if ((s_command_length == (sizeof(stepper_disable) - 1U)) &&
        (memcmp(s_command_line, stepper_disable, sizeof(stepper_disable) - 1U) == 0))
    {
        if (Stepper_Disable() == STEPPER_STATUS_OK)
        {
            Protocol_SendOk();
        }
        else
        {
            Protocol_SendError();
        }
        return true;
    }

    if ((s_command_length == (sizeof(stepper_stop) - 1U)) &&
        (memcmp(s_command_line, stepper_stop, sizeof(stepper_stop) - 1U) == 0))
    {
        if (Stepper_Stop() == STEPPER_STATUS_OK)
        {
            Protocol_SendOk();
        }
        else
        {
            Protocol_SendError();
        }
        return true;
    }

    if ((s_command_length == (sizeof(stepper_query) - 1U)) &&
        (memcmp(s_command_line, stepper_query, sizeof(stepper_query) - 1U) == 0))
    {
        Protocol_SendStepperStatus();
        return true;
    }

    if ((s_command_length > (sizeof(stepper_move_prefix) - 1U)) &&
        (memcmp(s_command_line,
                stepper_move_prefix,
                sizeof(stepper_move_prefix) - 1U) == 0))
    {
#if (RAW_BENCH_COMMANDS_ENABLE == 1)
        int32_t steps;
        uint32_t frequency_hz;

        if (Protocol_ParseSigned32AndUnsigned32((uint16_t)(sizeof(stepper_move_prefix) - 1U),
                                               &steps,
                                               &frequency_hz) &&
            (Stepper_MoveSteps(steps, frequency_hz) == STEPPER_STATUS_OK))
        {
            Protocol_SendOk();
        }
        else
        {
            Protocol_SendError();
        }
#else
        Protocol_SendError();
#endif
        return true;
    }

    if ((s_command_length == (sizeof(servo_query) - 1U)) &&
        (memcmp(s_command_line, servo_query, sizeof(servo_query) - 1U) == 0))
    {
        if (!Servo_IsInitialized())
        {
            Protocol_SendError();
        }
        else if (Servo_IsAngleValid())
        {
            Protocol_SendServoAngle();
        }
        else
        {
            Protocol_SendServoRawPulse();
        }
        return true;
    }

    if ((s_command_length > (sizeof(servo_prefix) - 1U)) &&
        (memcmp(s_command_line, servo_prefix, sizeof(servo_prefix) - 1U) == 0))
    {
#if (RAW_BENCH_COMMANDS_ENABLE == 1)
        if (Protocol_ParseDecimal((uint16_t)(sizeof(servo_prefix) - 1U), &value) &&
            (value <= SERVO_MAX_ANGLE_DEG) &&
            (PitchAxis_SetRawServoAngleMilliDeg((int32_t)value * 1000) == PITCH_AXIS_STATUS_OK))
        {
            Protocol_SendOk();
        }
        else
        {
            Protocol_SendError();
        }
#else
        Protocol_SendError();
#endif
        return true;
    }

    if ((s_command_length > (sizeof(servo_us_prefix) - 1U)) &&
        (memcmp(s_command_line, servo_us_prefix, sizeof(servo_us_prefix) - 1U) == 0))
    {
#if (RAW_BENCH_COMMANDS_ENABLE == 1)
        if (Protocol_ParseDecimal((uint16_t)(sizeof(servo_us_prefix) - 1U), &value) &&
            (PitchAxis_SetRawPulseUs(value) == PITCH_AXIS_STATUS_OK))
        {
            Protocol_SendOk();
        }
        else
        {
            Protocol_SendError();
        }
#else
        Protocol_SendError();
#endif
        return true;
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
    uint16_t processed_bytes = 0U;

    while (processed_bytes < PROTOCOL_MAX_BYTES_PER_PROCESS)
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

        processed_bytes++;
        Protocol_ProcessByte(byte);
    }
}

uint32_t Protocol_GetTxErrorCount(void)
{
    return s_tx_error_count;
}

#include "protocol.h"

#include "hc04.h"
#include "control_debug_config.h"
#include "main.h"
#include "pitch_axis.h"
#include "project_config.h"
#include "servo.h"
#include "sort_task.h"
#include "sort_sequence.h"
#include "stepper.h"
#include "yaw_axis.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>

static uint8_t s_command_line[PROTOCOL_COMMAND_BUFFER_SIZE];
static uint16_t s_command_length;
static bool s_discard_until_lf;
static bool s_reply_error_after_resync;
static uint32_t s_tx_error_count;
static bool s_ready_heartbeat_started;
static uint32_t s_last_ready_heartbeat_tick;

volatile uint32_t protocol_last_rx_action_id;
volatile uint32_t protocol_last_rx_box;
volatile char protocol_last_tx_type;
volatile uint32_t protocol_valid_frame_count;
volatile uint32_t protocol_crc_error_count;
volatile uint32_t protocol_format_error_count;
volatile uint32_t protocol_duplicate_count;
volatile uint32_t protocol_id_conflict_count;
volatile uint32_t protocol_busy_reject_count;
volatile uint32_t protocol_bad_box_count;

static void Protocol_Send(const uint8_t *data, uint16_t length)
{
    if (HC04_Send(data, length) != HC04_STATUS_OK)
    {
        s_tx_error_count++;
    }
}

static void Protocol_Increment(volatile uint32_t *counter)
{
    if ((counter != NULL) && (*counter < UINT32_MAX))
    {
        (*counter)++;
    }
}

uint8_t Protocol_Crc8Atm(const uint8_t *data, uint16_t length)
{
    uint8_t crc = 0U;
    uint16_t index;

    if ((data == NULL) && (length != 0U))
    {
        return 0U;
    }

    for (index = 0U; index < length; index++)
    {
        uint8_t bit;
        crc ^= data[index];
        for (bit = 0U; bit < 8U; bit++)
        {
            crc = ((crc & 0x80U) != 0U)
                      ? (uint8_t)((crc << 1U) ^ 0x07U)
                      : (uint8_t)(crc << 1U);
        }
    }
    return crc;
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

static void Protocol_SendFrame(const uint8_t *content, uint16_t content_length, char type)
{
    static const uint8_t hex_digits[] = "0123456789ABCDEF";
    uint8_t frame[40];
    uint16_t length = 0U;
    uint8_t crc;

    if ((content == NULL) || (content_length == 0U) ||
        ((uint32_t)content_length + 5U > sizeof(frame)))
    {
        return;
    }

    frame[length++] = (uint8_t)'$';
    (void)memcpy(&frame[length], content, content_length);
    length = (uint16_t)(length + content_length);
    crc = Protocol_Crc8Atm(content, content_length);
    frame[length++] = (uint8_t)'*';
    frame[length++] = hex_digits[(crc >> 4U) & 0x0FU];
    frame[length++] = hex_digits[crc & 0x0FU];
    frame[length++] = (uint8_t)'\n';
    protocol_last_tx_type = type;
    Protocol_Send(frame, length);
}

static uint16_t Protocol_BeginFrameContent(uint8_t type, uint8_t *content)
{
    content[0] = type;
    return 1U;
}

void Protocol_SendAck(uint32_t action_id)
{
    uint8_t content[20];
    uint16_t length = Protocol_BeginFrameContent((uint8_t)'A', content);

    content[length++] = (uint8_t)',';
    length = (uint16_t)(length + Protocol_AppendUnsigned32(action_id, &content[length]));
    Protocol_SendFrame(content, length, 'A');
}

void Protocol_SendDone(uint32_t action_id, uint8_t result)
{
    uint8_t content[24];
    uint16_t length = Protocol_BeginFrameContent((uint8_t)'D', content);

    if (result > 1U)
    {
        result = 1U;
    }
    content[length++] = (uint8_t)',';
    length = (uint16_t)(length + Protocol_AppendUnsigned32(action_id, &content[length]));
    content[length++] = (uint8_t)',';
    length = (uint16_t)(length + Protocol_AppendDecimal(result, &content[length]));
    Protocol_SendFrame(content, length, 'D');
}

void Protocol_SendNack(uint32_t action_id, ProtocolNackReason_t reason)
{
    static const uint8_t *const reason_text[] = {
        (const uint8_t *)"BUSY",
        (const uint8_t *)"FAULT",
        (const uint8_t *)"BAD_BOX",
        (const uint8_t *)"ID_CONFLICT"};
    static const uint8_t reason_length[] = {4U, 5U, 7U, 11U};
    uint8_t content[36];
    uint16_t length = Protocol_BeginFrameContent((uint8_t)'N', content);

    if ((uint32_t)reason >= (sizeof(reason_text) / sizeof(reason_text[0])))
    {
        reason = PROTOCOL_NACK_FAULT;
    }
    content[length++] = (uint8_t)',';
    length = (uint16_t)(length + Protocol_AppendUnsigned32(action_id, &content[length]));
    content[length++] = (uint8_t)',';
    (void)memcpy(&content[length], reason_text[reason], reason_length[reason]);
    length = (uint16_t)(length + reason_length[reason]);
    Protocol_SendFrame(content, length, 'N');
}

void Protocol_SendReady(void)
{
    static const uint8_t ready[] = {'R'};
    Protocol_SendFrame(ready, (uint16_t)sizeof(ready), 'R');
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

static void Protocol_RecordFormatError(void)
{
    Protocol_Increment(&protocol_format_error_count);
}

static int32_t Protocol_HexValue(uint8_t character)
{
    if ((character >= (uint8_t)'0') && (character <= (uint8_t)'9'))
    {
        return (int32_t)(character - (uint8_t)'0');
    }
    if ((character >= (uint8_t)'A') && (character <= (uint8_t)'F'))
    {
        return (int32_t)(character - (uint8_t)'A') + 10;
    }
    if ((character >= (uint8_t)'a') && (character <= (uint8_t)'f'))
    {
        return (int32_t)(character - (uint8_t)'a') + 10;
    }
    return -1;
}

static bool Protocol_ParseUnsignedField(const uint8_t *field,
                                        uint16_t field_length,
                                        uint32_t *value)
{
    uint16_t index;
    uint32_t parsed = 0U;

    if ((field == NULL) || (value == NULL) || (field_length == 0U))
    {
        return false;
    }
    for (index = 0U; index < field_length; index++)
    {
        uint32_t digit;
        if ((field[index] < (uint8_t)'0') || (field[index] > (uint8_t)'9'))
        {
            return false;
        }
        digit = (uint32_t)(field[index] - (uint8_t)'0');
        if (parsed > ((UINT32_MAX - digit) / 10U))
        {
            return false;
        }
        parsed = (parsed * 10U) + digit;
    }
    *value = parsed;
    return true;
}

static void Protocol_ReplyForExistingAction(uint32_t action_id, uint8_t box_id)
{
    SortActionRecord_t record;

    if (!SortTask_FindAction(action_id, &record))
    {
        return;
    }

    if (record.box != box_id)
    {
        Protocol_Increment(&protocol_id_conflict_count);
        Protocol_SendNack(action_id, PROTOCOL_NACK_ID_CONFLICT);
    }
    else
    {
        Protocol_Increment(&protocol_duplicate_count);
        if (record.completed)
        {
            Protocol_SendDone(action_id, record.result);
        }
        else
        {
            Protocol_SendAck(action_id);
        }
    }
}

static void Protocol_HandleSortAction(uint32_t action_id, uint32_t box_value)
{
    SortActionRecord_t existing_record;
    SortAcceptStatus_t accept_status;

    protocol_last_rx_action_id = action_id;
    protocol_last_rx_box = box_value;

    if ((box_value < 1U) || (box_value > 4U))
    {
        Protocol_Increment(&protocol_bad_box_count);
        Protocol_SendNack(action_id, PROTOCOL_NACK_BAD_BOX);
        return;
    }

    if (SortTask_FindAction(action_id, &existing_record))
    {
        Protocol_ReplyForExistingAction(action_id, (uint8_t)box_value);
        return;
    }

    if (SortSequence_IsEnabled())
    {
        Protocol_Increment(&protocol_busy_reject_count);
        Protocol_SendNack(action_id, PROTOCOL_NACK_BUSY);
        return;
    }

    accept_status = SortTask_AcceptAction(action_id, (uint8_t)box_value);
    switch (accept_status)
    {
        case SORT_ACCEPT_ACCEPTED:
            /* Reserve the action, acknowledge it, then expose YAW_MOVE to the task. */
            Protocol_SendAck(action_id);
            SortTask_StartAcceptedAction(HAL_GetTick());
            break;

        case SORT_ACCEPT_BAD_BOX:
            Protocol_Increment(&protocol_bad_box_count);
            Protocol_SendNack(action_id, PROTOCOL_NACK_BAD_BOX);
            break;

        case SORT_ACCEPT_BUSY:
            Protocol_Increment(&protocol_busy_reject_count);
            Protocol_SendNack(action_id, PROTOCOL_NACK_BUSY);
            break;

        case SORT_ACCEPT_DUPLICATE:
            Protocol_ReplyForExistingAction(action_id, (uint8_t)box_value);
            break;

        case SORT_ACCEPT_FAULT:
        default:
            Protocol_SendNack(action_id, PROTOCOL_NACK_FAULT);
            break;
    }
}

static void Protocol_ProcessFramedLine(void)
{
    uint16_t star_position;
    uint16_t content_length;
    uint16_t index;
    int32_t high_nibble;
    int32_t low_nibble;
    uint8_t received_crc;
    uint8_t calculated_crc;
    const uint8_t *content;
    uint16_t second_comma = 0U;
    uint32_t action_id;
    uint32_t box_value;

    if ((s_command_length < 5U) || (s_command_line[0] != (uint8_t)'$'))
    {
        Protocol_RecordFormatError();
        return;
    }
    star_position = (uint16_t)(s_command_length - 3U);
    if (s_command_line[star_position] != (uint8_t)'*')
    {
        Protocol_RecordFormatError();
        return;
    }
    for (index = 1U; index < star_position; index++)
    {
        if (s_command_line[index] == (uint8_t)'*')
        {
            Protocol_RecordFormatError();
            return;
        }
    }

    high_nibble = Protocol_HexValue(s_command_line[star_position + 1U]);
    low_nibble = Protocol_HexValue(s_command_line[star_position + 2U]);
    if ((high_nibble < 0) || (low_nibble < 0))
    {
        Protocol_RecordFormatError();
        return;
    }
    received_crc = (uint8_t)(((uint8_t)high_nibble << 4U) | (uint8_t)low_nibble);
    calculated_crc = Protocol_Crc8Atm(&s_command_line[1],
                                      (uint16_t)(star_position - 1U));
    if (received_crc != calculated_crc)
    {
        Protocol_Increment(&protocol_crc_error_count);
        return;
    }
    Protocol_Increment(&protocol_valid_frame_count);

    content = &s_command_line[1];
    content_length = (uint16_t)(star_position - 1U);
    if ((content_length < 5U) || (content[0] != (uint8_t)'S') ||
        (content[1] != (uint8_t)','))
    {
        Protocol_RecordFormatError();
        return;
    }
    for (index = 2U; index < content_length; index++)
    {
        if (content[index] == (uint8_t)',')
        {
            if (second_comma != 0U)
            {
                Protocol_RecordFormatError();
                return;
            }
            second_comma = index;
        }
    }
    if ((second_comma <= 2U) || (second_comma >= (content_length - 1U)) ||
        !Protocol_ParseUnsignedField(&content[2],
                                     (uint16_t)(second_comma - 2U),
                                     &action_id) ||
        !Protocol_ParseUnsignedField(&content[second_comma + 1U],
                                     (uint16_t)(content_length - second_comma - 1U),
                                     &box_value))
    {
        Protocol_RecordFormatError();
        return;
    }

    Protocol_HandleSortAction(action_id, box_value);
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
    bool motion_command;
#if (RAW_BENCH_COMMANDS_ENABLE == 1)
    uint16_t value;
#endif

    motion_command =
        ((s_command_length > (sizeof(pitch_prefix) - 1U)) &&
         (memcmp(s_command_line, pitch_prefix, sizeof(pitch_prefix) - 1U) == 0)) ||
        ((s_command_length == (sizeof(stepper_enable) - 1U)) &&
         (memcmp(s_command_line, stepper_enable, sizeof(stepper_enable) - 1U) == 0)) ||
        ((s_command_length == (sizeof(stepper_disable) - 1U)) &&
         (memcmp(s_command_line, stepper_disable, sizeof(stepper_disable) - 1U) == 0)) ||
        ((s_command_length == (sizeof(stepper_stop) - 1U)) &&
         (memcmp(s_command_line, stepper_stop, sizeof(stepper_stop) - 1U) == 0)) ||
        ((s_command_length > (sizeof(stepper_move_prefix) - 1U)) &&
         (memcmp(s_command_line,
                 stepper_move_prefix,
                 sizeof(stepper_move_prefix) - 1U) == 0)) ||
        ((s_command_length > (sizeof(servo_prefix) - 1U)) &&
         (memcmp(s_command_line, servo_prefix, sizeof(servo_prefix) - 1U) == 0)) ||
        ((s_command_length > (sizeof(servo_us_prefix) - 1U)) &&
         (memcmp(s_command_line,
                 servo_us_prefix,
                 sizeof(servo_us_prefix) - 1U) == 0));

    if ((SortTask_IsBusy() || SortSequence_IsEnabled()) && motion_command &&
        !((s_command_length == (sizeof(stepper_stop) - 1U)) &&
          (memcmp(s_command_line, stepper_stop, sizeof(stepper_stop) - 1U) == 0)))
    {
        Protocol_SendError();
        return true;
    }

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
        if (YawAxis_Enable() == YAW_AXIS_STATUS_OK)
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
        if (YawAxis_Disable() == YAW_AXIS_STATUS_OK)
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
        if (YawAxis_Stop() == YAW_AXIS_STATUS_OK)
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
            (YawAxis_MoveRelativePulses(steps, frequency_hz) == YAW_AXIS_STATUS_OK))
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
        if ((s_command_length > 0U) &&
            (s_command_line[0] == (uint8_t)'$'))
        {
            Protocol_RecordFormatError();
            s_command_length = 0U;
            s_discard_until_lf = true;
            s_reply_error_after_resync = false;
        }
        return;
    }
    if (byte == (uint8_t)'\n')
    {
        if (s_command_length != 0U)
        {
            s_command_line[s_command_length] = 0U;
            if (s_command_line[0] == (uint8_t)'$')
            {
                Protocol_ProcessFramedLine();
            }
            else if (!Protocol_ExecuteCommand())
            {
                Protocol_SendError();
            }
            s_command_length = 0U;
        }
        return;
    }

    if (s_command_length >= (PROTOCOL_COMMAND_BUFFER_SIZE - 1U))
    {
        const bool framed_line = (s_command_length > 0U) &&
                                 (s_command_line[0] == (uint8_t)'$');
        s_command_length = 0U;
        s_discard_until_lf = true;
        s_reply_error_after_resync = !framed_line;
        if (framed_line)
        {
            Protocol_RecordFormatError();
        }
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
    s_ready_heartbeat_started = false;
    s_last_ready_heartbeat_tick = 0U;
    protocol_last_rx_action_id = 0U;
    protocol_last_rx_box = 0U;
    protocol_last_tx_type = '\0';
    protocol_valid_frame_count = 0U;
    protocol_crc_error_count = 0U;
    protocol_format_error_count = 0U;
    protocol_duplicate_count = 0U;
    protocol_id_conflict_count = 0U;
    protocol_busy_reject_count = 0U;
    protocol_bad_box_count = 0U;
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

    if (!SortSequence_IsEnabled() && SortTask_IsReady())
    {
        const uint32_t now_ms = HAL_GetTick();
        if (!s_ready_heartbeat_started ||
            ((uint32_t)(now_ms - s_last_ready_heartbeat_tick) >=
             SORT_READY_HEARTBEAT_MS))
        {
            Protocol_SendReady();
            s_last_ready_heartbeat_tick = now_ms;
            s_ready_heartbeat_started = true;
        }
    }
    else
    {
        s_ready_heartbeat_started = false;
    }
}

uint32_t Protocol_GetTxErrorCount(void)
{
    return s_tx_error_count;
}

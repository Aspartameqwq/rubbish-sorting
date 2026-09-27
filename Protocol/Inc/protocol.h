#ifndef PROTOCOL_H
#define PROTOCOL_H

#include <stdbool.h>
#include <stdint.h>

typedef enum
{
    PROTOCOL_NACK_BUSY = 0,
    PROTOCOL_NACK_FAULT,
    PROTOCOL_NACK_BAD_BOX,
    PROTOCOL_NACK_ID_CONFLICT
} ProtocolNackReason_t;

/* Ozone watch counters and most recent framed protocol fields. */
extern volatile uint32_t protocol_last_rx_action_id;
extern volatile uint32_t protocol_last_rx_box;
extern volatile char protocol_last_tx_type;
extern volatile uint32_t protocol_valid_frame_count;
extern volatile uint32_t protocol_crc_error_count;
extern volatile uint32_t protocol_format_error_count;
extern volatile uint32_t protocol_duplicate_count;
extern volatile uint32_t protocol_id_conflict_count;
extern volatile uint32_t protocol_busy_reject_count;
extern volatile uint32_t protocol_bad_box_count;

void Protocol_Init(void);
void Protocol_Process(void);
uint8_t Protocol_Crc8Atm(const uint8_t *data, uint16_t length);
void Protocol_SendAck(uint32_t action_id);
void Protocol_SendDone(uint32_t action_id, uint8_t result);
void Protocol_SendNack(uint32_t action_id, ProtocolNackReason_t reason);
void Protocol_SendReady(void);
uint32_t Protocol_GetTxErrorCount(void);

#endif /* PROTOCOL_H */

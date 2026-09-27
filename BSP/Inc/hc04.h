#ifndef HC04_H
#define HC04_H

#include <stdint.h>

typedef enum
{
    HC04_STATUS_OK = 0,
    HC04_STATUS_NO_DATA,
    HC04_STATUS_INVALID_ARGUMENT,
    HC04_STATUS_OVERFLOW,
    HC04_STATUS_NOT_INITIALIZED,
    HC04_STATUS_CONFIGURATION_ERROR,
    HC04_STATUS_HAL_ERROR
} HC04Status;

HC04Status HC04_Init(void);
void HC04_Process(void);
HC04Status HC04_ReadByte(uint8_t *byte);
HC04Status HC04_Send(const uint8_t *data, uint16_t length);
uint32_t HC04_GetRxOverflowCount(void);
uint32_t HC04_GetRxErrorCount(void);

#endif /* HC04_H */

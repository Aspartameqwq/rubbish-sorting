#ifndef PROTOCOL_H
#define PROTOCOL_H

#include <stdint.h>

void Protocol_Init(void);
void Protocol_Process(void);
uint32_t Protocol_GetTxErrorCount(void);

#endif /* PROTOCOL_H */

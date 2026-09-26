#ifndef TEST_FAKES_H
#define TEST_FAKES_H

#include <stddef.h>
#include <stdint.h>

void TestFakes_ResetUart(void);
void TestFakes_FeedUart(const char *text);
size_t TestFakes_UartPending(void);
const char *TestFakes_TxData(void);
void TestFakes_ClearTx(void);
void TestFakes_SetTick(uint32_t tick_ms);
uint32_t TestFakes_GetTick(void);
uint32_t TestFakes_GetPwmStartCount(void);
uint32_t TestFakes_GetPwmStopCount(void);

#endif /* TEST_FAKES_H */

#ifndef DEBUG_STATE_H
#define DEBUG_STATE_H

#include "control_debug_config.h"

void Debug_Init(void);
/* Called from the application loop with its single SystemTime snapshot. */
void Debug_Process(uint32_t now_ms, uint32_t app_health_flags);

#endif /* DEBUG_STATE_H */

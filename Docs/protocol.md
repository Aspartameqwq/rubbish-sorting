# UART text protocol

## Status

This is the first-round protocol contract implemented by `Protocol/Src/protocol.c` and transported by `BSP/Src/hc04.c`. Keep this document synchronized with parser behavior and command responses.

## Transport

- Transport: HC-04 UART bridge over USART1, TX PA9 / RX PA10.
- UART format: asynchronous, 8 data bits, no parity, 1 stop bit, no hardware flow control.
- Baud: `HC04_BAUDRATE` in `Config/project_config.h`, currently 115200 as a `TO_BE_CONFIRMED` test default matching generated `usart.c`. The value must match the actual module/peer configuration.
- RX: DMA1 Channel 5, byte-wide, circular. The HAL Receive-to-Idle API enables UART IDLE events; DMA HT/TC callbacks track buffer wraps while `HC04_Process()` samples NDTR in the main loop. Command framing uses LF.
- TX: blocking HAL transmit is allowed only inside the HC-04 driver, with a finite configured timeout (`HC04_TX_TIMEOUT_MS`). No `HAL_MAX_DELAY`.

## Framing and grammar

Each command is one ASCII line ending in LF (`\n`). CRLF is accepted by ignoring CR (`\r`). A command is processed only after LF. Empty lines may be ignored.

```text
PING
SERVO <unsigned-decimal-angle>
SERVO?
SERVO_US <unsigned-decimal-pulse>
```

For numeric commands, one or more ASCII spaces separate the keyword from the number; the parser accepts repeated separator spaces and trailing spaces. `PING` and `SERVO?` must match exactly. Tabs, signs, and additional tokens are rejected. Numeric fields must contain at least one digit and fit in the target integer type. Parse with explicit length and overflow checks; do not use unbounded string operations.

The command line uses `PROTOCOL_COMMAND_BUFFER_SIZE` (64 bytes including its terminating NUL if the implementation stores one). A too-long line is marked invalid and discarded through the next LF. It must not be partially executed. The parser then resumes with the following line.

## Commands and responses

| Command | Action | Success response | Invalid response |
|---|---|---|---|
| `PING` | Verify command link | `PONG\r\n` | `ERR\r\n` |
| `SERVO <0..270>` | Set requested logical angle after validation | `OK\r\n` | `ERR\r\n` |
| `SERVO?` | Query last accepted logical angle | `SERVO <angle>\r\n` | `ERR\r\n` if state is unavailable |
| `SERVO_US <pulse>` | Set pulse after configured safety-range validation | `OK\r\n` | `ERR\r\n` |

No invalid or out-of-range value is silently clamped. For the initial `SERVO_US` implementation, the permitted range comes from `SERVO_MIN_PULSE_US` to `SERVO_MAX_PULSE_US`; these are provisional bench values and require calibration. The `SERVO` endpoint mapping is not evidence of the servo's true mechanical endpoints.

## Error, overflow, and recovery behavior

- Reject missing numbers, negative signs, alphabetic values, trailing garbage, integer overflow, and values outside configured bounds.
- Reject unknown commands and malformed token counts with `ERR`.
- On protocol line overflow, discard bytes through the next LF and reject that line. Do not allow the tail of an oversized line to become a new command.
- On a DMA overrun or UART/DMA receive error, discard the incomplete command, resynchronize at LF, update transport error/overflow counters, and allow RX DMA to recover without an infinite wait or long delay.
- The ISR/callback records event and error state only; command parsing and Servo calls happen from the main loop.

## Examples

```text
RX: PING\r\n
TX: PONG\r\n

RX: SERVO 135\n
TX: OK\r\n

RX: SERVO?\n
TX: SERVO 135\r\n

RX: SERVO_US 1500\n
TX: OK\r\n

RX: SERVO 500\n
TX: ERR\r\n
```

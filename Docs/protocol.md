# UART text protocol

## Transport and framing

- HC-04 UART bridge on USART1 TX PA9 / RX PA10, asynchronous 8N1, no flow control.
- Baud comes from `HC04_BAUDRATE` (115200 generated test default, still `TO_BE_CONFIRMED` with the actual module and peer). Driver reports a configuration error if generated USART baud differs; it does not reinitialize USART at runtime.
- RX uses DMA1 Channel 5 circular mode. A command is one ASCII line terminated by LF (`\n`); CR (`\r`) is ignored, so CRLF is accepted.
- Each `Protocol_Process()` invocation handles at most `PROTOCOL_MAX_BYTES_PER_PROCESS` bytes (64 by default). Command storage is bounded by `PROTOCOL_COMMAND_BUFFER_SIZE` (64 including the terminator).
- A too-long line is discarded through the next LF and produces `ERR`. DMA overrun/error similarly invalidates the partial command and waits for LF before resynchronizing.

## Grammar

```text
PING
SERVO <unsigned-decimal-angle>
SERVO?
SERVO_US <unsigned-decimal-pulse>
STEPPER ENABLE
STEPPER DISABLE
STEPPER MOVE <signed-decimal-steps> <unsigned-decimal-frequency>
STEPPER STOP
STEPPER?
```

Numeric fields require digits and explicit overflow/range validation. Servo fields are unsigned. Step count accepts an optional `+` or `-` and the full `int32_t` range, including `-2147483648`; frequency is unsigned and must be in the configured 20–10,000 steps/s initial software range. One or more ASCII spaces separate tokens; repeated and trailing spaces are accepted. Tabs, missing fields, overflow, extra tokens, and trailing non-space data are rejected.

## Commands and responses

| Command | Action | Success response | Invalid/unavailable |
|---|---|---|---|
| `PING` | Check command link | `PONG\r\n` | `ERR\r\n` |
| `SERVO <0..270>` | Set and retain the requested logical angle | `OK\r\n` | `ERR\r\n` |
| `SERVO?` | Query Servo state | `SERVO <angle>\r\n` | `SERVO RAW <pulse>\r\n` after `SERVO_US`; `ERR\r\n` before initialization |
| `SERVO_US <pulse>` | Set raw calibrated pulse in the configured range | `OK\r\n` | `ERR\r\n` |
| `STEPPER ENABLE` | Enable the TB6600 interface | `OK\r\n` | `ERR\r\n` |
| `STEPPER DISABLE` | Stop at a complete pulse boundary, then deassert ENA | `OK\r\n` | `ERR\r\n` |
| `STEPPER MOVE <steps> <frequency>` | Schedule an open-loop finite move | `OK\r\n` | `ERR\r\n` |
| `STEPPER STOP` | Gracefully stop at the next completed PUL active width | `OK\r\n` | `ERR\r\n` |
| `STEPPER?` | Query current firmware state | `STEPPER <STATE> POS=<n> REM=<n> FREQ=<n>\r\n` | State is `UNINITIALIZED` or `FAULT` when applicable |

`STEPPER MOVE 0 <valid-frequency>` is a successful no-op. Positive steps map to configured forward direction; negative steps map to reverse. A move is rejected while another move is setting direction, running or stopping, while disabled, or if the resulting commanded position would exceed `int32_t` bounds.

Example:

```text
RX: STEPPER ENABLE\n
TX: OK\r\n

RX: STEPPER MOVE -800 1000\n
TX: OK\r\n

RX: STEPPER?\n
TX: STEPPER RUNNING POS=0 REM=800 FREQ=1000\r\n
```

`POS` is the firmware's count of completed pulse-finished events, not a measured shaft position. After a move completes, remaining steps and current requested frequency reset to zero. A host-visible status response does not imply closed-loop position feedback.

## Numeric and recovery rules

- Reject signs or non-digits for Servo commands, missing Stepper fields, `int32_t` overflow, `uint32_t` frequency overflow, out-of-range frequencies, extra tokens, and unknown commands.
- `INT32_MIN` magnitude is calculated through a wider signed intermediate; it is never passed through `abs(int32_t)`.
- Never silently clamp a command.
- No command is partially executed after a too-long line. The parser drops through LF and then resumes with the next line.
- Parsing and replies run on the main loop. UART/DMA callbacks do not parse commands or send replies.
- HC-04 TX errors are counted internally; all protocol responses use the BSP transport and finite timeout.

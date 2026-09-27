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

Numeric fields require digits and explicit overflow/range validation. Servo fields are unsigned. Step count accepts an optional `+` or `-` and the full `int32_t` range, including `-2147483648`; frequency is unsigned and must be in the configured 20–10,000 PUL pulses/s initial software range. One or more ASCII spaces separate tokens; repeated and trailing spaces are accepted. Tabs, missing fields, overflow, extra tokens, and trailing non-space data are rejected.

## Commands and responses

| Command | Action | Success response | Invalid/unavailable |
|---|---|---|---|
| `PING` | Check command link | `PONG\r\n` | `ERR\r\n` |
| `SERVO <0..270>` | Set PitchAxis target in whole degrees; active Pitch software limits reject out-of-range requests | `OK\r\n` | `ERR\r\n` |
| `SERVO?` | Query Servo state | `SERVO <angle>\r\n` | `SERVO RAW <pulse>\r\n` after `SERVO_US`; `ERR\r\n` before initialization |
| `SERVO_US <pulse>` | Set a raw Pitch calibration pulse within the configured min/max; invalidates angle telemetry | `OK\r\n` | `ERR\r\n` |
| `STEPPER ENABLE` | Enable the TB6600 interface | `OK\r\n` | `ERR\r\n` |
| `STEPPER DISABLE` | Stop at a complete pulse boundary, then deassert ENA | `OK\r\n` | `ERR\r\n` |
| `STEPPER MOVE <steps> <frequency>` | Schedule an open-loop finite move | `OK\r\n` | `ERR\r\n` |
| `STEPPER STOP` | Gracefully stop at the next completed PUL active width | `OK\r\n` | `ERR\r\n` |
| `STEPPER?` | Query current firmware state | `STEPPER <STATE> POS=<n> REM=<n> FREQ=<n>\r\n` | State is `UNINITIALIZED` or `FAULT` when applicable |

`STEPPER MOVE 0 <valid-frequency>` is a successful no-op. The signed `steps` value is a count of PUL pulses, not degrees or motor full steps; frequency is PUL pulses per second. With the selected 8-microstep setting, 1600 pulses nominally correspond to one revolution of the reported 1.8° motor if directly coupled. Firmware does not read the switches. Positive pulses map to configured logical forward direction; negative pulses map to reverse. A move is rejected while another move is setting direction, running or stopping, while disabled, or if the resulting commanded position would exceed `int32_t` bounds.

## Scope: bench/debug commands

`STEPPER ENABLE/DISABLE/MOVE/STOP/?` is a raw open-loop bench interface to the low-level Stepper state. In particular, `STEPPER MOVE` bypasses YawAxis's manual-reference and software-limit checks. `SERVO_US` is a bounded raw calibration interface; it bypasses angle soft limits and invalidates the Pitch angle estimate. Use them only for controlled bring-up/debugging. Application or future K230 angle commands must call `PitchAxis`/`YawAxis` APIs; see [axis-control.md](axis-control.md) and [debugging.md](debugging.md).

`SERVO <degrees>` is already routed through PitchAxis, but the UART syntax has whole-degree resolution. Ozone's `g_debug_command` accepts millidegrees for finer logical targets. `SERVO?` reports the integer logical Servo target when angle mode is valid, or the raw pulse after calibration mode.

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

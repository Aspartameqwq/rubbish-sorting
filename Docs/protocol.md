# UART text protocol

## Transport and framing

- HC-04 UART bridge on USART1 TX PA9 / RX PA10, asynchronous 8N1, no flow control.
- Baud comes from `HC04_BAUDRATE` (115200 generated test default; confirm it against the actual module and peer). The driver reports a configuration error if generated USART baud differs; it does not reinitialize USART at runtime.
- RX uses DMA1 Channel 5 circular mode. A command is one ASCII line terminated by LF (`\n`); CR (`\r`) is ignored, so CRLF is accepted.
- Each `Protocol_Process()` invocation handles at most `PROTOCOL_MAX_BYTES_PER_PROCESS` bytes (64 by default). Command storage is bounded by `PROTOCOL_COMMAND_BUFFER_SIZE` (64 including the terminator).
- A too-long line is discarded through the next LF and produces `ERR`. DMA overrun/error invalidates the partial command and waits for LF before resynchronizing.

## Grammar

```text
PING
PITCH <signed-millidegrees>
SERVO?
SERVO <unsigned-degrees>          # Debug bench command
SERVO_US <unsigned-microseconds>  # Debug bench command
STEPPER ENABLE                    # Debug bench command
STEPPER DISABLE
STEPPER MOVE <signed-steps> <unsigned-frequency> # Debug bench command
STEPPER STOP
STEPPER?
```

`PITCH` accepts an optional `+` or `-` and an `int32_t` integer number of millidegrees. The PitchAxis then enforces the fixed `-30000..+30000 mdeg` range. Servo/bench fields are unsigned. Step count accepts the full `int32_t` range, including `-2147483648`; frequency is unsigned and must be within the initial 20–10,000 PUL/s software range. One or more ASCII spaces separate tokens; repeated and trailing spaces are accepted. Tabs, missing fields, overflow, extra tokens and trailing non-space data are rejected.

## Commands and responses

| Command | Action | Success response | Invalid/unavailable |
|---|---|---|---|
| `PING` | Check command link | `PONG\r\n` | `ERR\r\n` |
| `PITCH <signed-mdeg>` | Schedule a relative-to-horizontal Pitch target through PitchAxis | `OK\r\n` | `ERR\r\n` |
| `SERVO?` | Read current Servo state | `SERVO <angle>\r\n` or `SERVO RAW <pulse>\r\n` | `ERR\r\n` before initialization |
| `SERVO <0..270>` | Debug-only absolute Servo-angle bench request; converted to Pitch and checked against ±30° | `OK\r\n` | `ERR\r\n` |
| `SERVO_US <pulse>` | Debug-only raw PWM bench request; inverse-mapped to Pitch and checked against ±30° | `OK\r\n` | `ERR\r\n` |
| `STEPPER ENABLE` | Debug-only enable through YawAxis; reference must still be set before motion | `OK\r\n` | `ERR\r\n` |
| `STEPPER DISABLE` | Disable through YawAxis; invalidates cable reference | `OK\r\n` | `ERR\r\n` |
| `STEPPER MOVE <steps> <frequency>` | Debug-only relative-pulse move through YawAxis hard cable checks | `OK\r\n` | `ERR\r\n` |
| `STEPPER STOP` | Gracefully stop at the next complete pulse boundary; keeps a valid reference | `OK\r\n` | `ERR\r\n` |
| `STEPPER?` | Read current firmware state | `STEPPER <STATE> POS=<n> REM=<n> FREQ=<n>\r\n` | State is `UNINITIALIZED` or `FAULT` when applicable |

`PITCH 0` means horizontal; `PITCH 5000` and `PITCH -5000` request ±5°. `OK` means the request passed software validation and was scheduled. It is not a motion-complete reply. Pitch moves toward the target with a nonblocking linear response; its default duration is 1000 ms and the configurable range is 200–5000 ms.

Pitch requests outside `-30000..+30000 mdeg` are rejected and never clamped. The hard range cannot be disabled. Raw Servo angle and pulse inputs pass through PitchAxis conversion and the same limit check. Release returns `ERR` for `SERVO <angle>`, `SERVO_US`, `STEPPER ENABLE` and `STEPPER MOVE` because those bench actuation paths are compiled out. `STEPPER DISABLE` and `STEPPER STOP` remain available for recovery; DISABLE invalidates Yaw reference, while STOP preserves it.

`STEPPER MOVE 0 <valid-frequency>` is a successful no-op in Debug only when the Stepper is enabled and Yaw cable reference is valid. Its signed `steps` count represents PUL pulses, not degrees or full steps; frequency is PUL pulses per second. With the selected 8-microstep setting, 1600 pulses nominally correspond to one revolution of the reported 1.8° motor if directly coupled. Firmware does not read the switches. Positive pulses map to configured logical forward direction; negative pulses map to reverse. Every bench move calls `YawAxis_MoveRelativePulses()`, which checks reference and the final relative pulse position against the mandatory `-800..+800 PUL` cable range before scheduling motion.

There is no UART zero-setting command. Before motion, manually place the mechanism at cable neutral while the driver is disabled and issue Ozone `DEBUG_CMD_SET_YAW_ZERO`; then enable and use the UART move command. After `STEPPER DISABLE`, establish cable zero again before another move. Absolute angle targets use the same finite linear coordinate: +170° to -170° travels about -340°, never the +20° wrapped path.

Yaw angle targets currently go through `YawAxis` APIs or the Debug Ozone mailbox; no normal UART `YAW` command is defined. See [axis-control.md](axis-control.md) and [debugging.md](debugging.md).

## Examples

```text
RX: PITCH 0\n
TX: OK\r\n

RX: PITCH -5000\n
TX: OK\r\n

RX: PITCH 30001\n
TX: ERR\r\n

RX: STEPPER ENABLE\n   # Debug build only
TX: OK\r\n
```

`SERVO?` reports the Servo logical angle when angle mode is valid or the raw pulse while a bench pulse is active. `POS` in `STEPPER?` is the firmware's completed-pulse count, not a measured shaft position. A host-visible response does not imply motion or closed-loop feedback.

## Numeric and recovery rules

- Reject non-digits, `int32_t` overflow, `uint32_t` frequency overflow, out-of-range Pitch values, invalid Servo pulse/angle mappings, out-of-range frequencies, extra tokens and unknown commands.
- `INT32_MIN` magnitude is handled through a wider signed intermediate; it is never passed through `abs(int32_t)`.
- Never silently clamp a command.
- No command is partially executed after a too-long line. The parser drops through LF and then resumes with the next line.
- Parsing and replies run on the main loop. UART/DMA callbacks do not parse commands or send replies.
- HC-04 TX errors are counted internally; all protocol responses use the BSP transport and finite timeout.

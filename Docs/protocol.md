# UART protocols

## Four-box sorting frames

The HC-04 link also accepts the STM32 sorting frame `$S,action_id,box*CC\n`.
Every sorting response uses the same frame envelope: `$A,action_id*CC\n`,
`$D,action_id,result*CC\n`, `$N,action_id,reason*CC\n`, or `$R*CC\n`.
`CC` is two uppercase or lowercase hexadecimal digits containing CRC-8/ATM
(polynomial `0x07`, initial value `0x00`) over the ASCII content only; `$`, `*`,
the CRC characters, and LF are excluded. Framed messages require LF and reject
CR. A known CRC regression vector is `$S,42,3*6C\n`.

For example, `$S,105,3*<CRC>\n` selects Box 3. Boxes 1/3 share
`SORT_YAW_GROUP_13_MDEG`; boxes 2/4 share `SORT_YAW_GROUP_24_MDEG`. Each box's
Pitch direction is configured independently, and each paired direction must
be opposite. `ID_CONFLICT` means a retained action ID was received with a
different box. The STM32 rejects it without changing the original action.
The K230 peer must discard that conflicting request, re-identify after the
next safe Ready indication, and use a new action ID. This repository changes
only STM32 firmware; the peer behavior is a protocol contract.

See [K230_HC04_STM32_四盒分拣通信协议.md](K230_HC04_STM32_四盒分拣通信协议.md)
for the state sequence, commissioning values, timeouts, history behavior,
and debug symbols. Sorting remains disabled while the mechanical calibration
flag and axis calibration flags are unset, so no `R` is sent in the default
uncommissioned build.

An Ozone-controlled, fixed seven-action local test is described in
[debugging.md](debugging.md#seven-action-local-sorting-test). While it is
enabled, new framed sorting requests receive `BUSY` and Ready heartbeats are
suppressed; local test actions produce no peer `A` or `D` frames.

## Legacy text-command protocol

### Transport and framing

- HC-04 UART bridge on USART1 TX PA9 / RX PA10, asynchronous 8N1, no flow control.
- Baud comes from `HC04_BAUDRATE` (115200 generated test default; confirm it against the actual module and peer). The driver reports a configuration error if generated USART baud differs; it does not reinitialize USART at runtime.
- RX uses DMA1 Channel 5 circular mode. A legacy text command is one ASCII line terminated by LF (`\n`); CR (`\r`) is ignored, so CRLF is accepted. Sorting frames are parsed separately and require LF without CR.
- Each `Protocol_Process()` invocation handles at most `PROTOCOL_MAX_BYTES_PER_PROCESS` bytes (64 by default). Command storage is bounded by `PROTOCOL_COMMAND_BUFFER_SIZE` (64 including the terminator).
- A too-long line is discarded through the next LF and produces `ERR`. DMA overrun/error invalidates the partial command and waits for LF before resynchronizing.

### Legacy text-command grammar

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

`PITCH` accepts an optional `+` or `-` and an `int32_t` integer number of millidegrees. The PitchAxis then enforces the fixed `-45000..+45000 mdeg` range. Servo/bench fields are unsigned. Step count accepts the full `int32_t` range, including `-2147483648`; frequency is unsigned. The TB6600 layer accepts its configured 20–10,000 PUL/s range, while YawAxis commands are further restricted to the initial 20–500 PUL/s mechanism range. One or more ASCII spaces separate tokens; repeated and trailing spaces are accepted. Tabs, missing fields, overflow, extra tokens and trailing non-space data are rejected.

### Legacy text-command commands and responses

| Command | Action | Success response | Invalid/unavailable |
|---|---|---|---|
| `PING` | Check command link | `PONG\r\n` | `ERR\r\n` |
| `PITCH <signed-mdeg>` | Schedule a relative-to-horizontal Pitch target through PitchAxis | `OK\r\n` | `ERR\r\n` |
| `SERVO?` | Read current Servo state | `SERVO <angle>\r\n` or `SERVO RAW <pulse>\r\n` | `ERR\r\n` before initialization |
| `SERVO <0..270>` | Debug-only absolute Servo-angle bench request; converted to Pitch and checked against ±45° | `OK\r\n` | `ERR\r\n` |
| `SERVO_US <pulse>` | Debug-only raw PWM bench request; inverse-mapped to Pitch and checked against ±45° | `OK\r\n` | `ERR\r\n` |
| `STEPPER ENABLE` | Debug-only enable through YawAxis; reference must still be set before motion | `OK\r\n` | `ERR\r\n` |
| `STEPPER DISABLE` | Disable through YawAxis; if a graceful stop is needed, reference remains valid through STOPPING and is invalidated once DISABLED | `OK\r\n` | `ERR\r\n` |
| `STEPPER MOVE <steps> <frequency>` | Debug-only relative-pulse move through YawAxis cable and 20–500 PUL/s checks | `OK\r\n` | `ERR\r\n` |
| `STEPPER STOP` | Gracefully stop at the next complete pulse boundary; keeps a valid reference | `OK\r\n` | `ERR\r\n` |
| `STEPPER?` | Read current firmware state | `STEPPER <STATE> POS=<n> REM=<n> FREQ=<n>\r\n` | State is `UNINITIALIZED` or `FAULT` when applicable |

`PITCH 0` requests the bench-observed horizontal anchor at Servo 148° (approximately 1596 µs). `PITCH 5000` and `PITCH -5000` request ±5° relative to that anchor. `OK` means the request passed software validation and was scheduled. It is not a motion-complete reply. Pitch moves toward the target with a nonblocking linear response; its default duration is 1000 ms and the configurable range is 200–5000 ms. The new startup anchor takes effect after reflashing.

Pitch requests outside `-45000..+45000 mdeg` are rejected and never clamped. The hard range cannot be disabled. Raw Servo angle and pulse inputs pass through PitchAxis conversion and the same limit check. Release returns `ERR` for `SERVO <angle>`, `SERVO_US`, `STEPPER ENABLE` and `STEPPER MOVE` because those bench actuation paths are compiled out. `STEPPER DISABLE` and `STEPPER STOP` remain available for recovery. STOP preserves the reference; DISABLE invalidates it immediately when already idle, or after a requested graceful stop reaches `DISABLED`.

`STEPPER MOVE 0 <valid-frequency>` is a successful no-op in Debug only when the Stepper is enabled and Yaw cable reference is valid. Its signed `steps` count represents PUL pulses, not degrees or full steps; frequency is PUL pulses per second. The current `YAW_AXIS_PULSES_PER_REV=1600` configuration means pulses per Yaw output-axis revolution and assumes the selected 8-microstep row and 1:1 motor-to-platform coupling. The DIP settings and mechanical ratio are unverified. Positive pulses map to configured logical forward direction; negative pulses map to reverse. Every bench move calls `YawAxis_MoveRelativePulses()`, which checks reference, the final relative pulse position against the derived `-800..+800 PUL` cable range, and the Yaw-specific `20..500 PUL/s` rate before scheduling motion.

There is no UART zero-setting command. Application startup assumes the current disabled position is Yaw 0°; physically place the mechanism at cable neutral before power-up. This assumption does not move the motor or verify its angle. Then enable and use the UART move command. After `STEPPER DISABLE` has completed, establish cable zero again with Ozone `DEBUG_CMD_SET_YAW_ZERO` before another move. Absolute angle targets use the same finite linear coordinate: +170° to -170° travels about -340°, never the +20° wrapped path. The configured -180°..+180° range is a software command guard; do not treat it as verified physical travel until DIP, transmission ratio, and small-angle pulse scale have been checked.

Yaw angle targets currently go through `YawAxis` APIs or the Debug Ozone mailbox; no normal UART `YAW` command is defined. See [axis-control.md](axis-control.md) and [debugging.md](debugging.md).

### Examples

```text
RX: PITCH 0\n
TX: OK\r\n

RX: PITCH -5000\n
TX: OK\r\n

RX: PITCH 45001\n
TX: ERR\r\n

RX: STEPPER ENABLE\n   # Debug build only
TX: OK\r\n
```

`SERVO?` reports the Servo logical angle when angle mode is valid or the raw pulse while a bench pulse is active. `POS` in `STEPPER?` is the firmware's completed-pulse count, not a measured shaft position. A host-visible response does not imply motion or closed-loop feedback.

### Numeric and recovery rules

- Reject non-digits, `int32_t` overflow, `uint32_t` frequency overflow, out-of-range Pitch values, invalid Servo pulse/angle mappings, out-of-range frequencies, extra tokens and unknown commands.
- `INT32_MIN` magnitude is handled through a wider signed intermediate; it is never passed through `abs(int32_t)`.
- Never silently clamp a command.
- No command is partially executed after a too-long line. The parser drops through LF and then resumes with the next line.
- Parsing and replies run on the main loop. UART/DMA callbacks do not parse commands or send replies.
- HC-04 TX errors are counted internally; all protocol responses use the BSP transport and finite timeout.

# J-Link / Ozone debugging

## Build and load

Build the **Debug** preset and open the ELF with DWARF symbols in SEGGER Ozone:

```text
build/Debug/rubbish-sorting.elf
```

Use a J-Link SWD session for STM32F103C8T6. The Debug ELF exposes the single Watch object `g_control_debug`. SEGGER's [Ozone User Manual (UM08025)](https://www.segger.com/downloads/jlink) and [Watch-window expression guidance](https://kb.segger.com/Add_Expressions_to_the_Watch_Window) describe the relevant setup.

## Recommended Watch expressions

Add these fields from `Config/control_debug_config.h`:

```text
g_control_debug.state.snapshot_seq
g_control_debug.state.heartbeat
g_control_debug.state.tick_ms
g_control_debug.state.app_health_flags
g_control_debug.state.last_debug_command
g_control_debug.state.last_debug_result

g_control_debug.state.pitch.target_mdeg
g_control_debug.state.pitch.commanded_mdeg
g_control_debug.state.pitch.servo_target_mdeg
g_control_debug.state.pitch.servo_pulse_us
g_control_debug.state.pitch.measured_mdeg
g_control_debug.state.pitch.measurement_valid
g_control_debug.state.pitch.moving
g_control_debug.state.pitch.response_time_ms
g_control_debug.state.pitch.active_response_time_ms
g_control_debug.state.pitch.trajectory_elapsed_ms
g_control_debug.state.pitch.soft_limit_min_mdeg
g_control_debug.state.pitch.soft_limit_max_mdeg
g_control_debug.state.pitch.limit_reject_count
g_control_debug.state.pitch.tuning_reject_count
g_control_debug.state.pitch.status

g_control_debug.state.yaw.target_mdeg
g_control_debug.state.yaw.quantized_target_mdeg
g_control_debug.state.yaw.commanded_mdeg
g_control_debug.state.yaw.measured_mdeg
g_control_debug.state.yaw.commanded_position_pulses
g_control_debug.state.yaw.zero_offset_pulses
g_control_debug.state.yaw.remaining_pulses
g_control_debug.state.yaw.pulse_frequency_hz
g_control_debug.state.yaw.stepper_state
g_control_debug.state.yaw.enabled
g_control_debug.state.yaw.reference_state

g_control_debug.tuning.pitch_response_time_ms
g_control_debug.command.request_seq
g_control_debug.command.applied_seq
g_control_debug.command.command
g_control_debug.command.result
```

Angles are signed integer millidegrees. Pitch `0` means platform horizontal; `+5000` and `-5000` request ±5° around horizontal. Servo `130000 mdeg` is the current mechanically observed horizontal anchor. `commanded_mdeg` is the latest software command, not measured platform position. Pitch and Yaw `measured_mdeg` remain `INT32_MIN` with `measurement_valid=0`.

Telemetry refreshes every 20 ms. `snapshot_seq` is odd while a snapshot is being copied and even when stable. To validate a Watch sample, note the sequence, read the fields, then read the sequence again; accept the sample only if both values match and are even. `heartbeat` increments once per completed snapshot.

## Pitch examples

Use small initial movements and observe mechanism clearance:

```text
Horizontal:  pitch_target_mdeg = 0
Small side A: pitch_target_mdeg = 5000
Return:       pitch_target_mdeg = 0
Small side B: pitch_target_mdeg = -5000
Return:       pitch_target_mdeg = 0
```

The enforced Pitch range is `-30000` through `+30000` mdeg. Requests outside it return `DEBUG_RESULT_LIMIT`. Do not begin hardware checks at the endpoints. The software bound does not verify real mechanical clearance.

## Response-time tuning

The default response is 1000 ms; the accepted range is 200–5000 ms. In a Debug build, Ozone may write the next-move setting directly:

```text
g_control_debug.tuning.pitch_response_time_ms = 1200
```

The firmware validates this every main-loop pass. An invalid value is replaced with the last accepted value and increments `pitch.tuning_reject_count`. A currently active move keeps the duration it latched when its target was accepted; a new setting applies to the next Pitch target. Software limits are compile-time configuration and are not writable Ozone tuning values.

## Single-request command mailbox

`g_control_debug.command` is the only Ozone command input. Do not edit `g_control_debug.state`, Servo CCR, Stepper private state, timer registers or GPIO registers. Submit one request at a time:

1. Write command parameters.
2. Write `command` using a `DEBUG_CMD_*` value.
3. As the final write, increment `request_seq`.
4. Wait until `applied_seq == request_seq`; inspect `result` and `state.last_debug_result` before sending another request.

For a +5° Pitch target:

```text
g_control_debug.command.pitch_target_mdeg = 5000
g_control_debug.command.command = DEBUG_CMD_SET_PITCH_MDEG
g_control_debug.command.request_seq = g_control_debug.command.request_seq + 1
```

After processing, firmware writes `result`, clears `command.command` to `DEBUG_CMD_NONE`, and finally updates `applied_seq`. `state.last_debug_command` and `state.last_debug_result` preserve the completed command. Do not increment the sequence again until the current request is acknowledged.

| Value | Command | Parameters / behavior |
|---:|---|---|
| 1 | `DEBUG_CMD_SET_PITCH_MDEG` | Relative Pitch target; always checked against ±30° |
| 2 | `DEBUG_CMD_SET_PITCH_PULSE_US` | Bench pulse request; converted back to Pitch and checked against the same hard limit |
| 3 | `DEBUG_CMD_SET_YAW_MDEG` | Yaw target and frequency; requires manual reference and enabled idle Stepper |
| 4 | `DEBUG_CMD_SET_BOTH_MDEG` | Prevalidates Pitch and Yaw requests before scheduling both |
| 5 | `DEBUG_CMD_SET_YAW_ZERO` | Save current stationary Stepper count as manual zero; does not move the shaft |
| 6 | `DEBUG_CMD_YAW_ENABLE` | Enable the Stepper/TB6600 interface |
| 7 | `DEBUG_CMD_YAW_DISABLE` | Request Stepper disable |
| 8 | `DEBUG_CMD_YAW_STOP` | Graceful stop at a complete pulse boundary |
| 9 | `DEBUG_CMD_SET_PITCH_RESPONSE_MS` | Set the response time for the next Pitch target |

For command 9, write `pitch_response_time_ms` in the command block before incrementing the request sequence. Valid values are 200–5000 ms.

Before `DEBUG_CMD_SET_YAW_ZERO`, physically align the Yaw mechanism to its chosen zero while stopped and safe. MCU reset or a zero pulse count is not physical homing.

## Protocol bench-command gate and build policy

The normal text command is `PITCH <signed-mdeg>`, such as `PITCH 0` or `PITCH -5000`. `SERVO <degrees>`, `SERVO_US <pulse>` and raw `STEPPER ENABLE/MOVE` commands are bench interfaces. They are enabled only in Debug and their Pitch paths still enforce the Pitch hard limit. `STEPPER DISABLE` and `STEPPER STOP` remain available as stop/recovery commands. Release returns `ERR` for bench-only protocol commands.

The global `g_control_debug` symbol exists in Debug and Release ELFs. Debug command injection and `RAW_BENCH_COMMANDS_ENABLE` are enabled only for the Debug preset; Release acknowledges mailbox requests as disabled and compiles out raw bench actuation paths.

Verify the external symbol with:

```powershell
arm-none-eabi-nm build/Debug/rubbish-sorting.elf | Select-String 'g_control_debug'
```

`nm` confirms link-visible ELF data only; it does not confirm a J-Link session or physical movement.

## Hardware and CubeMX boundary

- Keep the mechanism secured and clear before any Ozone or UART motion request.
- Start at horizontal, then test ±5°; verify direction and clearance before trying ±10° or more.
- Software limits are command guards, not physical stops. Servo calibration and mechanism travel remain bench work.
- `.ioc` and `Core/*` are CubeMX-owned. After user regeneration, review `MX_TIM3_Init()` order and generated-source changes; do not commit machine-local package paths from generated CMake files.

ST's [STM32CubeMX User Manual UM1718](https://www.st.com/resource/en/user_manual/um1718-stm32cubemx-for-stm32-configuration-and-initialization-c-code-generation-stmicroelectronics.pdf) describes generation from the saved `.ioc` configuration.

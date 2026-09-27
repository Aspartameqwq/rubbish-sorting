# J-Link / Ozone debugging

## Build and load

Build the **Debug** preset and point SEGGER Ozone at the ELF with DWARF symbols:

```text
build/Debug/rubbish-sorting.elf
```

Create/select a device session for STM32F103C8, use the J-Link SWD interface, and load that ELF as the application image. Keep the Debug configuration's compiler-generated symbols; do not use a stripped Release ELF for this Watch workflow. The checked build verifies the firmware image and symbols, not J-Link connection or physical motion.

SEGGER's [Ozone User Manual (UM08025)](https://www.segger.com/downloads/jlink) is the primary setup reference. Ozone Watch can display global ELF symbols and expressions; see SEGGER's [Watch-window expression guidance](https://kb.segger.com/Add_Expressions_to_the_Watch_Window).

## Recommended Watch variables

Add these public expressions rather than searching private `static` module variables:

```text
g_debug_state.heartbeat
g_debug_state.tick_ms
g_debug_state.app_health_flags

g_debug_state.pitch.target_mdeg
g_debug_state.pitch.commanded_mdeg
g_debug_state.pitch.measured_mdeg
g_debug_state.pitch.measurement_valid
g_debug_state.pitch.pulse_us
g_debug_state.pitch.servo_enabled
g_debug_state.pitch.calibration_valid
g_debug_state.pitch.soft_limit_enabled
g_debug_state.pitch.soft_limit_min_mdeg
g_debug_state.pitch.soft_limit_max_mdeg
g_debug_state.pitch.limit_reject_count
g_debug_state.pitch.raw_pulse_mode
g_debug_state.pitch.status

g_debug_state.yaw.target_mdeg
g_debug_state.yaw.quantized_target_mdeg
g_debug_state.yaw.commanded_mdeg
g_debug_state.yaw.measured_mdeg
g_debug_state.yaw.measurement_valid
g_debug_state.yaw.commanded_position_pulses
g_debug_state.yaw.zero_offset_pulses
g_debug_state.yaw.remaining_pulses
g_debug_state.yaw.pulse_frequency_hz
g_debug_state.yaw.stepper_state
g_debug_state.yaw.enabled
g_debug_state.yaw.reference_state
g_debug_state.yaw.soft_limit_enabled
g_debug_state.yaw.soft_limit_min_mdeg
g_debug_state.yaw.soft_limit_max_mdeg
g_debug_state.yaw.limit_reject_count
g_debug_state.yaw.status

g_debug_command
```

`heartbeat` increments on each 20 ms snapshot; a frozen value can indicate that main-loop processing is not reaching Diagnostics. `tick_ms` and the health/status fields give context. The snapshot is a mirror only: never edit `g_debug_state` to control hardware.

Angles use signed integer mdeg. `measured_mdeg` is `INT32_MIN` (`-2147483648`) and `measurement_valid=0` until sensor feedback is implemented. Pitch commanded angle is derived from the configured PWM mapping; Yaw commanded angle is derived from counted pulses relative to its reference. Neither is a measured mechanical angle. `soft_limit_enabled=0` means the configured placeholder range is not active or calibrated protection.

## Debug command mailbox

`g_debug_command` is the only Ozone control input. Do not write Servo CCR, Stepper private state, timer registers, GPIO registers, or the telemetry mirror.

Submit one request at a time:

1. Write command parameters.
2. Write `command` using the `DEBUG_CMD_*` value from `Diagnostics/Inc/debug_state.h`.
3. As the final write, increment `request_seq`.
4. Wait until `applied_seq == request_seq`, then inspect `result` before sending another request.

Example: request Pitch 45°:

```text
g_debug_command.pitch_target_mdeg = 45000
g_debug_command.command = DEBUG_CMD_SET_PITCH_MDEG   // enum value 1
g_debug_command.request_seq = g_debug_command.request_seq + 1
```

The CPU checks the sequence each main-loop iteration, copies the request payload after observing the sequence change, calls the axis API, writes `result`, then acknowledges by copying the request sequence to `applied_seq`. Updating the sequence last prevents the CPU from acting on incomplete parameters. Do not queue a second request before the first is acknowledged.

Supported command values are:

| Value | Command | Parameters / behavior |
|---:|---|---|
| 1 | `DEBUG_CMD_SET_PITCH_MDEG` | `pitch_target_mdeg`; PitchAxis checks logical range and active soft limits |
| 2 | `DEBUG_CMD_SET_PITCH_PULSE_US` | `pitch_pulse_us`; raw calibration command bounded by configured pulse min/max, invalidates angle telemetry |
| 3 | `DEBUG_CMD_SET_YAW_MDEG` | `yaw_target_mdeg`, `yaw_frequency_hz`; requires manual reference, enabled idle Stepper and accepted limits |
| 4 | `DEBUG_CMD_SET_BOTH_MDEG` | Pitch and Yaw targets plus Yaw frequency; prevalidates both; a hardware failure after the Pitch write may return `DEBUG_RESULT_PARTIAL` |
| 5 | `DEBUG_CMD_SET_YAW_ZERO` | Save current idle Stepper count as manual zero; does not move the shaft or rewrite the Stepper count |
| 6 | `DEBUG_CMD_YAW_ENABLE` | Enable the Yaw Stepper/TB6600 interface |
| 7 | `DEBUG_CMD_YAW_DISABLE` | Request disable, finishing an active normal pulse first |
| 8 | `DEBUG_CMD_YAW_STOP` | Graceful stop at the next complete pulse boundary |

`result` values are declared as `DEBUG_RESULT_*` in the header. `DEBUG_RESULT_OK` is 0; other results report disabled control, invalid/unknown command, uninitialized axis, missing reference, limit rejection, busy/disabled Yaw, driver error or a partial `SET_BOTH_MDEG` result. The telemetry `pitch.status` and `yaw.status` fields use their respective `PITCH_AXIS_STATUS_*` and `YAW_AXIS_STATUS_*` enum values. Limit rejection counts and each axis's last status are copied into telemetry.

Before `DEBUG_CMD_SET_YAW_ZERO`, physically align the mechanism to the intended reference while it is stopped and mechanically safe. MCU reset or `Stepper commanded_position_pulses == 0` is not a physical home. Yaw absolute commands are rejected until a manual reference has been set.

## Build policy and symbols

Telemetry symbols are included in every firmware configuration. Command injection is compiled on only for the Debug preset (`DEBUG_CONTROL_ENABLE=1`); Release and other non-Debug presets set it to 0. A changed `request_seq` in a non-Debug build is acknowledged with `DEBUG_RESULT_DISABLED` and cannot move an actuator.

Confirm the ELF contains the two external symbols:

```powershell
arm-none-eabi-nm build/Debug/rubbish-sorting.elf | Select-String 'g_debug_state|g_debug_command'
```

Ozone watches named globals from ELF debug information; `nm` independently verifies external linkage. `nm` presence does not prove an Ozone/J-Link session or hardware action.

## Bring-up boundaries

- Keep the TB6600 24 V stage disconnected during the multi-pulse GPIO input-current screen described in [wiring.md](wiring.md).
- Keep the mechanism safe and clear before enabling the driver or requesting motion.
- `SERVO_US` and `STEPPER MOVE` text commands are raw bench interfaces; direct `STEPPER MOVE` bypasses YawAxis reference and software-limit policy.
- Debug commands call PitchAxis/YawAxis APIs. Debug does not bypass angle checks through raw actuator registers.
- Physical Servo calibration, electrical input current/logic recognition, motor direction, shaft angle and sensor feedback remain pending until measured.

## CubeMX generation provenance

ST's [STM32CubeMX User Manual UM1718](https://www.st.com/resource/en/user_manual/um1718-stm32cubemx-for-stm32-configuration-and-initialization-c-code-generation-stmicroelectronics.pdf) describes generating project code from the saved configuration. This repository treats `.ioc` and `Core/*` as CubeMX-owned; after regeneration, review `MX_TIM3_Init()` ordering and generated-source changes, but do not commit CubeMX's machine-local package paths from `cmake/stm32cubemx/CMakeLists.txt`.

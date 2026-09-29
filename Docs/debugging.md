# J-Link / Ozone debugging

## Connect and load

Build the **Debug** preset and open the ELF with DWARF symbols in SEGGER Ozone:

```text
build/Debug/rubbish-sorting.elf
```

Use an SWD session for STM32F103C8T6. Add the single top-level firmware object
`g_control_debug` to Watch and expand `state`, `command`, and `tuning` as needed.
To send a request or tune a value, edit the watched variable's **Value** cell;
keep its expression/name unchanged. The state fields refresh from the main loop
about every 20 ms. SEGGER's
[Ozone User Manual (UM08025)](https://www.segger.com/downloads/jlink) and
[Watch-window expression guidance](https://kb.segger.com/Add_Expressions_to_the_Watch_Window)
describe Watch setup. SEGGER's [Ozone application-debugging reference](https://www.segger.com/products/development-tools/ozone-j-link-debugger/technology/application-debugging/)
describes watched-variable windows and symbol editing. See [wiring.md](wiring.md)
for the SWD pins and staged hardware checks.

Debug-state layout version is 4. Always load the ELF produced by the same
firmware build as the connected target; do not use a version-3 ELF or saved
addresses with the appended fields.

`snapshot_seq` is a sequence lock: odd means a snapshot is being copied; even
means the copy finished. To validate one Watch sample, read the sequence, read
the fields, then read the sequence again. Accept the sample only when both
sequence reads are equal and even. `heartbeat` increments with each completed
snapshot.

## General state reference

| Watch field | Unit / values | Meaning |
|---|---|---|
| `g_control_debug.state.snapshot_seq` | count | Odd while updating; even after a complete snapshot |
| `g_control_debug.state.version` | version | Debug-state layout version; currently 4 |
| `g_control_debug.state.heartbeat` | count | Increments on each completed snapshot; a changing value indicates the main loop is servicing diagnostics |
| `g_control_debug.state.tick_ms` | ms | Shared system time from `HAL_GetTick()` through `SystemTime_GetMs()` |
| `g_control_debug.state.app_health_flags` | bitmask | Initialization faults recorded by `App_Init()`; zero means no recorded init fault |
| `g_control_debug.state.last_debug_command` | enum | Most recently acknowledged Ozone command |
| `g_control_debug.state.last_debug_result` | enum | Result of the most recently acknowledged Ozone command |

`app_health_flags` bits are: bit 0 Servo init, bit 1 HC-04 init, bit 2 TB6600
init, bit 3 Stepper init, bit 4 PitchAxis init, bit 5 YawAxis init. This is an
initialization summary, not a continuous hardware diagnostic.

## Pitch fields

All Pitch and Servo angle fields are signed integer millidegrees. Pitch is
relative to platform horizontal; Servo is the actuator's absolute logical
angle. The user confirmed horizontal at Servo 148° on the bench, so the
configured horizontal anchor is 148000 mdeg (approximately 1596 µs).
The user's 270° Servo reference maps 0°/135°/270° to 500/1500/2500 µs,
independently of this platform anchor. The confirmed Servo-to-platform angle
ratio is 1:1; positive Pitch tilted the platform backward in the live check.

| Watch field | Unit / values | Meaning |
|---|---|---|
| `state.pitch.target_mdeg` | mdeg | Requested Pitch relative to horizontal; invalid before initialization is `INT32_MIN` |
| `state.pitch.commanded_mdeg` | mdeg | Latest software Pitch command sent toward the target; not measured platform position |
| `state.pitch.servo_target_mdeg` | mdeg | Servo absolute logical target derived from the Pitch anchor and direction |
| `state.pitch.servo_pulse_us` | µs | Current Servo PWM compare value |
| `state.pitch.measured_mdeg` | mdeg | Sensor measurement; currently `INT32_MIN` |
| `state.pitch.measurement_valid` | 0/1 | Currently 0; no Pitch sensor is implemented |
| `state.pitch.moving` | 0/1 | A time-based Pitch trajectory is active |
| `state.pitch.response_time_ms` | ms | Response duration to use for the next accepted target |
| `state.pitch.active_response_time_ms` | ms | Duration latched by the currently active trajectory |
| `state.pitch.trajectory_elapsed_ms` | ms | Elapsed time in the active trajectory, capped at its duration |
| `state.pitch.soft_limit_min_mdeg` | mdeg | Mandatory lower command limit; default -45000 |
| `state.pitch.soft_limit_max_mdeg` | mdeg | Mandatory upper command limit; default +45000 |
| `state.pitch.limit_reject_count` | count | Pitch requests rejected by the software limit |
| `state.pitch.tuning_reject_count` | count | Invalid direct Ozone response-time values rejected and restored |
| `state.pitch.servo_enabled` | 0/1 | Servo PWM output is enabled in the BSP |
| `state.pitch.calibration_valid` | 0/1 | Exact Servo/linkage calibration flag; currently 0 |
| `state.pitch.raw_pulse_mode` | 0/1 | Last Pitch actuation was a Debug raw pulse/angle request |
| `state.pitch.status` | `PitchAxisStatus` | Most recent PitchAxis result; see the enum in `Control/Inc/pitch_axis.h` |

Normal Pitch control remains `-45000..+45000 mdeg`; do not disable it. The
software limit is a command guard, not a physical stop or a measured safe range.

## Yaw fields

| Watch field | Unit / values | Meaning |
|---|---|---|
| `state.yaw.target_mdeg` | mdeg | Requested Yaw relative to the manually chosen zero |
| `state.yaw.quantized_target_mdeg` | mdeg | Requested angle rounded to the nearest realizable PUL position |
| `state.yaw.commanded_mdeg` | mdeg | Open-loop angle estimated from completed pulses and zero offset |
| `state.yaw.measured_mdeg` | mdeg | Sensor measurement; currently `INT32_MIN` |
| `state.yaw.measurement_valid` | 0/1 | Currently 0; no Yaw sensor is implemented |
| `state.yaw.commanded_position_pulses` | PUL | Firmware's completed-pulse position count |
| `state.yaw.zero_offset_pulses` | PUL | Pulse count stored by `DEBUG_CMD_SET_YAW_ZERO` |
| `state.yaw.remaining_pulses` | PUL | Pulses remaining in the current move |
| `state.yaw.pulse_frequency_hz` | PUL/s | Current requested pulse frequency |
| `state.yaw.stepper_state` | enum | `UNINITIALIZED=0`, `DISABLED=1`, `IDLE=2`, `DIR_SETUP=3`, `RUNNING=4`, `STOPPING=5`, `FAULT=6` |
| `state.yaw.enabled` | 0/1 | Stepper/TB6600 interface enabled state |
| `state.yaw.reference_state` | enum | `INVALID=0`, `MANUAL=1`; HOMED/SENSOR are reserved states, not implemented workflows |
| `state.yaw.soft_limit_min_mdeg` / `soft_limit_max_mdeg` | mdeg | Deprecated compatibility names; report the mandatory cable limits -180000 / +180000 |
| `state.yaw.soft_limit_enabled` | 0/1 | Deprecated compatibility field; always 1 because cable limits cannot be disabled |
| `state.yaw.limit_reject_count` | count | Compatibility name for the number of Yaw cable-limit rejections |
| `state.yaw.status` | `YawAxisStatus` | Most recent YawAxis result; see `Control/Inc/yaw_axis.h` |
| `state.yaw.cable_limit_min_mdeg` / `cable_limit_max_mdeg` | mdeg | Mandatory cable range: -180000..+180000 |
| `state.yaw.cable_limit_min_pulses` / `cable_limit_max_pulses` | PUL | Mandatory range relative to cable zero: -800..+800 |
| `state.yaw.cable_margin_to_min_mdeg` | mdeg | Current open-loop Yaw estimate minus the lower angle limit; `INT32_MIN` if reference is invalid |
| `state.yaw.cable_margin_to_max_mdeg` | mdeg | Upper angle limit minus the current open-loop Yaw estimate; `INT32_MIN` if reference is invalid |
| `state.yaw.cable_remaining_negative_pulses` | PUL | Remaining allowed negative-direction pulses; 0 if reference is invalid |
| `state.yaw.cable_remaining_positive_pulses` | PUL | Remaining allowed positive-direction pulses; 0 if reference is invalid |
| `state.yaw.cable_limit_reject_count` | count | Yaw cable-limit rejection counter; same counter as the compatibility field above |
| `state.yaw.axis_pulses_per_rev` | PUL / output-axis revolution | Configured pulse count for one full Yaw platform revolution; current assumption is 1600 |
| `state.yaw.mdeg_per_pulse` | mdeg/PUL | Current configured conversion; 225 under the 1600 PUL/output-revolution assumption |
| `state.yaw.axis_scale_verified` | 0/1 | Configuration flag; currently 0 until DIP, transmission ratio and small-angle motion are bench checked |
| `state.yaw.frequency_min_hz` | PUL/s | Yaw mechanism's current minimum command rate; 20 |
| `state.yaw.frequency_max_hz` | PUL/s | Initial conservative Yaw command-rate ceiling; 500, not a driver or motor rating |
| `state.yaw.cable_margin_valid` | 0/1 | Whether software cable angle and pulse margins have a valid manual reference; 0 means do not interpret margins as position |

Yaw has no slip ring. Its 0° coordinate is the manually established natural
Pitch-cable route, and its configured software range is -180°..+180° around
that zero. The position is a finite linear coordinate: +170° to -170° is about
-340°, never the +20° shortest path. `axis_pulses_per_rev` counts PUL pulses
for one complete revolution of the Yaw output axis/platform. The current 1600
value is an assumption from the reported 1.8° motor, selected 8-microstep
row, and assumed 1:1 motor-to-platform coupling. `axis_scale_verified == 0`
means the DIP positions, mechanical ratio, and pulse-to-platform angle have
not been bench verified. Consequently, `mdeg_per_pulse == 225` and the
derived ±800 PUL limits are configured estimates, not confirmed physical
scale or travel.

YawAxis currently accepts 20–500 PUL/s. The 500 PUL/s maximum is an initial
conservative bring-up limit, separate from the TB6600 layer's wider timer
range. At the assumed 1600 PUL/output revolution it corresponds nominally to
18.75 rpm; it is not a motor or driver rating. Increase it only after review
of measured, reliable motion.

`cable_margin_valid == 1` means the software has a manual cable-neutral
reference and its margins are calculated from completed open-loop pulse
counts. It does not mean the physical shaft angle was measured. Missed steps,
stall, mechanical slip, incorrect DIP settings, transmission-ratio mismatch,
or hand movement while unpowered can make commanded Yaw differ from actual
Yaw. `measured_mdeg` remains `INT32_MIN` and `measurement_valid` remains 0.

When `reference_state == INVALID`, `cable_margin_valid == 0`,
`target_mdeg`, `quantized_target_mdeg`,
`commanded_mdeg`, and both angle margins are `INT32_MIN`; do not interpret that
sentinel as 0°. Pulse margins are 0 while reference is invalid. At boot, the
Stepper count being zero does not establish cable neutral.

## Response-time tuning

The only recommended direct Ozone tuning field is:

```text
g_control_debug.tuning.pitch_response_time_ms
```

Edit its Value cell to set the duration for the **next** Pitch target. For
example, enter `1500` for a 1500 ms response. Default is 1000 ms; valid
values are 200–5000 ms. An active target keeps its latched
`active_response_time_ms`; changing `response_time_ms` does not speed up or slow
down that move. Invalid values are restored to the last accepted value and
increment `state.pitch.tuning_reject_count`.

For a secured mechanism, tune gradually and observe response and clearance:

```text
1500 → 1200 → 1000 → 800 → 600 ms
```

Do not start with 200 ms. These are software settings, not measured mechanical
specifications. Pitch limits are compile-time settings and are not writable in
Ozone.

## Command mailbox

`g_control_debug.command` is the only Ozone command input. Do not write state,
Servo compare registers, Stepper internals, timer registers, or GPIO registers.
Send only one request at a time:

1. Write the command parameters.
2. Write the `command` enum value.
3. As the last write, increment `request_seq`.
4. Wait until `applied_seq == request_seq`; inspect `result` and the saved last
   command/result before submitting another request.

| Mailbox field | Unit / values | Meaning |
|---|---|---|
| `command.request_seq` | sequence | Increment after all request fields are written |
| `command.applied_seq` | sequence | Sequence of the last request completed by firmware |
| `command.command` | `DebugCommandType` | Request enum; firmware clears it to `NONE` after execution |
| `command.pitch_target_mdeg` | mdeg | Relative Pitch target parameter |
| `command.yaw_target_mdeg` | mdeg | Yaw target parameter |
| `command.yaw_frequency_hz` | PUL/s | Yaw move frequency parameter; accepted range is 20–500 |
| `command.pitch_pulse_us` | µs | Debug raw Servo pulse parameter; still Pitch-limit checked |
| `command.pitch_response_time_ms` | ms | Parameter for command 9 |
| `command.result` | `DebugCommandResult` | Result for the acknowledged request |

| Value | Command | Behavior |
|---:|---|---|
| 0 | `DEBUG_CMD_NONE` | No command |
| 1 | `DEBUG_CMD_SET_PITCH_MDEG` | Set relative Pitch target |
| 2 | `DEBUG_CMD_SET_PITCH_PULSE_US` | Debug raw pulse; inverse mapped and checked against Pitch limits |
| 3 | `DEBUG_CMD_SET_YAW_MDEG` | Set Yaw target and frequency; requires manual reference and enabled idle Stepper |
| 4 | `DEBUG_CMD_SET_BOTH_MDEG` | Prevalidate both axis requests, then schedule them |
| 5 | `DEBUG_CMD_SET_YAW_ZERO` | Save current pulse count as cable-neutral zero; accepted only while Stepper is disabled |
| 6 | `DEBUG_CMD_YAW_ENABLE` | Enable the Stepper/TB6600 interface |
| 7 | `DEBUG_CMD_YAW_DISABLE` | Disable the Stepper; if moving, keep the reference through `STOPPING`, then invalidate it on `DISABLED` |
| 8 | `DEBUG_CMD_YAW_STOP` | Gracefully stop at a complete pulse boundary; preserve valid reference |
| 9 | `DEBUG_CMD_SET_PITCH_RESPONSE_MS` | Set response time for the next Pitch target |

| Result value | Meaning |
|---:|---|
| 0 | `DEBUG_RESULT_OK` |
| -1 | `DEBUG_RESULT_DISABLED` |
| -2 | `DEBUG_RESULT_UNKNOWN_COMMAND` |
| -3 | `DEBUG_RESULT_INVALID_ARGUMENT` |
| -4 | `DEBUG_RESULT_NOT_INITIALIZED` |
| -5 | `DEBUG_RESULT_NOT_REFERENCED` |
| -6 | `DEBUG_RESULT_LIMIT` |
| -7 | `DEBUG_RESULT_BUSY` |
| -8 | `DEBUG_RESULT_AXIS_DISABLED` |
| -9 | `DEBUG_RESULT_DRIVER_ERROR` |
| -10 | `DEBUG_RESULT_PARTIAL` |
| -11 | `DEBUG_RESULT_INVALID_STATE` |

For `state.pitch.status`, values are `OK=0`, `INVALID_ARGUMENT=1`,
`NOT_INITIALIZED=2`, `LIMIT=3`, `DISABLED=4`, and `DRIVER_ERROR=5`. For
`state.yaw.status`, values are `OK=0`, `INVALID_ARGUMENT=1`,
`NOT_INITIALIZED=2`, `NOT_REFERENCED=3`, `LIMIT=4`, `BUSY=5`, `DISABLED=6`,
`DRIVER_ERROR=7`, and `INVALID_STATE=8`.

After execution, firmware stores `result`, updates `state.last_debug_command`
and `state.last_debug_result`, clears `command.command` to `DEBUG_CMD_NONE`,
then updates `applied_seq`. Wait for that acknowledgment before writing the
next request.

### Pitch command examples

Horizontal uses target `0`; Pitch +5° uses `5000`; Pitch -5° uses `-5000`.
Edit the Watch Value cells in this order (the final `request_seq` edit submits
the request):

| Field | Value for horizontal | Value for +5° |
|---|---:|---:|
| `g_control_debug.command.pitch_target_mdeg` | 0 | 5000 |
| `g_control_debug.command.command` | 1 | 1 |
| `g_control_debug.command.request_seq` | Current value + 1 | Current value + 1 |

Use -5000 for -5°. Do not submit the next command until `applied_seq` matches
the incremented request sequence. `command.result` is updated on acknowledgment;
the saved `state.last_debug_*` fields appear with the next stable state snapshot.
Observe `target_mdeg`, `commanded_mdeg`, `servo_target_mdeg`, `servo_pulse_us`,
and `moving`; wait for the acknowledgment and `moving == 0` before issuing the
next physical move. For a software-only limit check, +45001 mdeg should return
`DEBUG_RESULT_LIMIT` and increment `limit_reject_count`; do not use endpoint
tests as the first hardware motion test.

To change response time through command 9, set
`g_control_debug.command.pitch_response_time_ms` to a value from 200 through
5000, set `g_control_debug.command.command` to 9, then increment
`g_control_debug.command.request_seq` as the last edit. The direct tuning field
is simpler when only the next-move duration needs to change.

### Yaw cable-zero and +5° example

At boot the Yaw reference is invalid, regardless of the Stepper pulse count.
With the Stepper/TB6600 disabled, manually place the mechanism where the Pitch
cable has its natural route and no visible twist. Then send command 5
(`DEBUG_CMD_SET_YAW_ZERO`). Confirm `reference_state == MANUAL`,
`commanded_mdeg == 0`, and that `zero_offset_pulses` records the current
Stepper count. Send command 6 (`DEBUG_CMD_YAW_ENABLE`) and confirm
`enabled == 1`; only then request a Yaw target. Setting zero while enabled,
busy, stopping, or faulted returns `DEBUG_RESULT_INVALID_STATE`.

`DEBUG_CMD_YAW_STOP` preserves the reference after a graceful stop. An idle
`DEBUG_CMD_YAW_DISABLE` invalidates it immediately. If disable is requested
while pulses are running, Ozone keeps the final commanded angle and margins
valid through `STOPPING`; the reference becomes invalid after the Stepper
reaches `DISABLED`. A fault invalidates the reference. To move after disable,
re-establish cable neutral, set zero while disabled, then enable. Re-enabling
alone does not restore the reference.

For +5° at a conservative 100 PUL/s, edit these Value cells in order and
increment `request_seq` last:

| Field | Value |
|---|---:|
| `g_control_debug.command.yaw_target_mdeg` | 5000 |
| `g_control_debug.command.yaw_frequency_hz` | 100 |
| `g_control_debug.command.command` | 3 |
| `g_control_debug.command.request_seq` | Current value + 1 |

At the current unverified scale of 225 mdeg/PUL, this rounds to about 22 PUL,
or 4950 mdeg. Observe
`quantized_target_mdeg`, `remaining_pulses` and `commanded_position_pulses`.
This is still open-loop pulse bookkeeping, not confirmation of actual shaft
angle. The +5°, 0°, -5°, 0° sequence is the first motion check; increase the
range gradually only after each move and cable clearance are confirmed. Do not
start with ±180°.

The Yaw Watch fields to keep visible are:

```text
g_control_debug.state.yaw.target_mdeg
g_control_debug.state.yaw.quantized_target_mdeg
g_control_debug.state.yaw.commanded_mdeg
g_control_debug.state.yaw.commanded_position_pulses
g_control_debug.state.yaw.zero_offset_pulses
g_control_debug.state.yaw.reference_state
g_control_debug.state.yaw.enabled
g_control_debug.state.yaw.stepper_state
g_control_debug.state.yaw.remaining_pulses
g_control_debug.state.yaw.pulse_frequency_hz
g_control_debug.state.yaw.cable_limit_min_mdeg
g_control_debug.state.yaw.cable_limit_max_mdeg
g_control_debug.state.yaw.cable_limit_min_pulses
g_control_debug.state.yaw.cable_limit_max_pulses
g_control_debug.state.yaw.cable_margin_to_min_mdeg
g_control_debug.state.yaw.cable_margin_to_max_mdeg
g_control_debug.state.yaw.cable_remaining_negative_pulses
g_control_debug.state.yaw.cable_remaining_positive_pulses
g_control_debug.state.yaw.cable_limit_reject_count
g_control_debug.state.yaw.status
```

## Staged first hardware debugging

Use [wiring.md](wiring.md) as the wiring and measurement checklist. Secure the
mechanism and keep driver power disconnected until its GPIO input-current and
loaded-voltage checks pass.

### A. Power up without motion

- Confirm `heartbeat` increments and `app_health_flags == 0`.
- Check Pitch target/command are 0 and Servo PWM is enabled.
- Check `state.yaw.enabled == 0`; measure that PB13 is HIGH on the corrected active-low ENA configuration.
- Confirm PA6 has no PUL edges while idle.

### B. Servo and Pitch direction

- Begin with Pitch 0° and confirm the platform is horizontal at the recorded
  148° Servo anchor (approximately 1596 µs). If the mechanical zero changes,
  use Debug `command.pitch_pulse_us` and `command.command = 2` to find the
  new level pulse, then update `PITCH_LEVEL_SERVO_MDEG` and reflash.
- Request `+5000`, wait for completion, return to 0, then request `-5000` and
  return to 0. Verify directions and clearances at every step.
- Stop if motion binds or moves unexpectedly. Do not start at ±45°.

### C. Adjust Pitch response

- After safe small movements work, try 1500, 1000, then 800 ms, waiting for
  each move to complete before the next request.
- Compare configured and active response times; an in-progress trajectory
  retains its original active duration.
- Do not begin at the 200 ms minimum.

### D. Check TB6600 electrical signals

- Follow the no-driver-power current and voltage screening in [wiring.md](wiring.md).
- Keep the 8 mA per-signal project gate; measure PUL with a suitable series
  shunt/scope and verify the approximately 10 µs pulse and low-rate waveform.
- Apply the selected 12 V driver supply only after the input-current and MCU output-voltage checks pass.
- Verify ENA and DIR physical behavior; software state alone is not proof.

### E. First Yaw motion

- Only after the electrical and powered-driver checks pass, keep the motor
  secured, disable the Stepper, align cable neutral, set zero in Ozone, and follow the zero/enable/+5°
  sequence above at low frequency.
- Check +5°, return to 0°, then -5° and return to 0°. Record direction,
  movement, missed-step observations and measured pulses in `wiring.md`.

Do not begin with Pitch ±45°, Yaw 180°, or 10 kHz Stepper commands.

## Debug/Release and CubeMX boundary

Normal UART Pitch control is `PITCH <signed-mdeg>`. `SERVO <degrees>`,
`SERVO_US <pulse>` and `STEPPER ENABLE/MOVE` are Debug-only bench interfaces;
Pitch raw requests still pass through the same ±45° check and Stepper moves
pass through YawAxis cable-reference, pulse-limit and 20–500 PUL/s checks. Release returns
`ERR` for those raw actuation commands. `STEPPER DISABLE` and `STEPPER STOP`
remain available for recovery; a completed DISABLE invalidates the cable
reference, while a graceful pending disable keeps it valid through `STOPPING`.

The `g_control_debug` ELF symbol exists in Debug and Release. Full axis command
injection and raw bench actuation are enabled only in Debug; Release retains
the limited Yaw zero/enable/disable/stop commissioning commands described
below. Confirm symbol presence with:

```powershell
arm-none-eabi-nm build/Debug/rubbish-sorting.elf | Select-String 'g_control_debug'
```

`nm` confirms linked data only; it does not confirm an Ozone session or physical
movement. `.ioc` and `Core/*` remain CubeMX-owned. Review generated source and
initialization order after the user regenerates CubeMX output; do not commit
machine-local package paths from generated CMake files.

## Seven-action local sorting test

The local test runs the fixed box sequence `1, 2, 3, 4, 3, 2, 1` through the
normal four-box sorting state machine. The next action starts 5000 ms after the
previous action has **fully returned to Yaw/Pitch HOME**. The interval is
measured from completion, so motion time is additional. The sequence runs once;
it does not repeat automatically.

Add `g_sort_sequence` to Ozone Watch. Edit only these two input fields:

| Field | Value | Meaning |
|---|---:|---|
| `g_sort_sequence.enabled` | `0` or `1` | Default `0`; write `1` to start and `0` to prevent further actions. Re-arm with `0` then `1` to restart from box 1. |
| `g_sort_sequence.interval_ms` | `1000`–`60000` | Default `5000`; may be changed while waiting. Values outside this range pause scheduling with status `INVALID_INTERVAL`. |

Watch `status`, `next_index`, `completed_count`, `active_box`, `last_result`,
and `last_completion_tick_ms`. `next_index` is zero-based. Status values are
`DISABLED=0`, `WAIT_READY=1`, `RUNNING=2`, `WAIT_INTERVAL=3`, `COMPLETE=4`,
`FAULT=5`, and `INVALID_INTERVAL=6`. `WAIT_READY` means the normal sorting
readiness check has not passed. A failed action latches `FAULT`; the existing
sorting fault cannot be cleared by toggling this test switch. Writing `0`
while an action is running stops **future** actions; the accepted action
continues to its normal completion or fault. `DEBUG_CMD_YAW_STOP` / `STEPPER
STOP` remain available for a deliberate motion stop.

The test requires the same mechanical and axis calibration as live four-box
sorting. The repository defaults keep these flags unset, so writing `enabled=1`
on an uncommissioned build leaves it at `WAIT_READY` and does not move the
actuators. First validate the installed box angles and Pitch directions,
configure the calibration values described in
[the four-box protocol](K230_HC04_STM32_四盒分拣通信协议.md), establish the Yaw
cable-neutral zero while disabled, enable Yaw, and confirm both axes are at
HOME. Do not set a verification flag from a software build alone.

This local test does not create action-history entries or emit synthetic
`A`/`D` frames to the HC-04 peer. While `enabled=1`, new framed sort requests
receive `N,...,BUSY`, Ready heartbeats are suppressed, and normal UART/Ozone
motion requests are rejected except Yaw STOP. The seven box values are in
`App/Src/sort_sequence.c`; change that array and rebuild to use another fixed
sequence. Ozone Watch does not edit the sequence itself.

## Four-box sorting state

The sorting task exposes direct ELF globals for Ozone Watch:

```text
sort_task.state
sort_task.action_id
sort_task.box
sort_task.yaw_target_mdeg
sort_task.pitch_direction
sort_task.pitch_target_mdeg
sort_task.state_enter_tick
sort_task.result
sort_task.action_valid
sort_task.action_completed
sort_task.fault_code
system_fault
yaw_is_home
yaw_is_at_target
pitch_is_home
pitch_is_at_target
protocol_last_rx_action_id
protocol_last_rx_box
protocol_last_tx_type
protocol_valid_frame_count
protocol_crc_error_count
protocol_format_error_count
protocol_duplicate_count
protocol_id_conflict_count
protocol_busy_reject_count
protocol_bad_box_count
```

Angles use signed millidegrees. `sort_task.state` follows `SortState_t` in
`App/Inc/sort_task.h`; `pitch_direction` is `+1` or `-1` only after mechanical
commissioning. For live axis positions and targets, reuse
`g_control_debug.state.yaw.commanded_mdeg`,
`g_control_debug.state.yaw.target_mdeg`,
`g_control_debug.state.pitch.commanded_mdeg`,
`g_control_debug.state.pitch.target_mdeg`, and
`g_control_debug.state.pitch.moving`. The Yaw and Pitch values are
command-position estimates: this board has no angle sensors, so they do not
prove that the platform or linkage physically reached the target.

In a Release build, the Ozone command mailbox keeps only Yaw commissioning
commands available: `DEBUG_CMD_SET_YAW_ZERO`, `DEBUG_CMD_YAW_ENABLE`,
`DEBUG_CMD_YAW_DISABLE`, and `DEBUG_CMD_YAW_STOP`. For each boot, keep the
driver disabled, manually align the cable route to its natural zero, set the
Yaw zero in Ozone, then enable the driver. This reference is volatile and must
be re-established after reset. The current project defines Pitch zero as
horizontal. Four-box sorting will not send `R` or accept a new sort action
until calibration macros, both axis calibration flags, and both home states
are valid. See the detailed protocol document for the exact compile-time
configuration and commissioning checklist.

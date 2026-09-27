# Pitch/Yaw axis control

## Coordinate model

The project axes are:

```text
Pitch → Servo → TIM2_CH1 / PA0
Yaw   → Stepper + TB6600 → TIM3_CH1 / PA6, PB12, PB13
```

Pitch and Servo use different coordinates. Pitch is relative to the platform's horizontal position; Servo is the actuator's logical angle.

| Coordinate | Meaning |
|---|---|
| Pitch `0 mdeg` | Platform horizontal at the bench-observed anchor |
| Servo `148000 mdeg` | Bench-observed Pitch zero; approximately 1596 µs |
| Pitch `+10000 mdeg` | Platform target 10° in the configured positive direction |

`PITCH_LEVEL_SERVO_MDEG` and `PITCH_SERVO_DIRECTION_SIGN` in [control_debug_config.h](../Config/control_debug_config.h) define the conversion:

```text
servo target = pitch horizontal anchor + direction sign × Pitch target
```

The user confirms a 1:1 Servo-to-platform angle magnitude. On the bench, raising Servo from 135° to 165° moved the platform from slightly forward tilt to pronounced backward tilt; the user then confirmed horizontal at Servo 148°. Thus `PITCH_SERVO_DIRECTION_SIGN=+1` makes positive Pitch tilt backward on this mechanism. The Servo uses the user-supplied 270° reference: 0°/135°/270° correspond to 500/1500/2500 µs. Its 135° midpoint is independent of `PITCH_LEVEL_SERVO_MDEG=148000`, the platform's horizontal Servo angle. PWM is quantized to whole microseconds; the configured 148° maps to 1596 µs.

For future horizontal trimming in a Debug build, use the Ozone mailbox field `g_control_debug.command.pitch_pulse_us` with `command.command = 2` (`DEBUG_CMD_SET_PITCH_PULSE_US`), then increment `command.request_seq` as the last write. The raw command is still checked against the ±45° Pitch window. Read `g_control_debug.state.pitch.servo_target_mdeg` for the Servo angle inferred from that pulse, then update `PITCH_LEVEL_SERVO_MDEG` in `Config/control_debug_config.h` and reflash to persist a new anchor. The current 148° observation was made with the UART `SERVO 148` Debug command; the runtime command alone does not persist across reset.

## Pitch limits and actuation path

Pitch commands are signed integer millidegrees. The enforced range is always:

```text
-45000 mdeg ≤ Pitch target ≤ +45000 mdeg
```

There is no runtime or build option to disable this Pitch limit. Compile-time checks ensure both configured endpoints map within the Servo logical range. With the observed 148° anchor and positive direction, the endpoints map to Servo 103° and 193°, or approximately 1263 and 1930 µs.

All application angle requests use `PitchAxis_SetTargetMilliDeg()`. Protocol, Ozone mailbox and future application callers pass through PitchAxis validation before Servo PWM changes. Raw absolute Servo angle and pulse commands are bench-only, compiled out of Release, and checked by converting the requested output back to Pitch coordinates before applying it. Raw pulse input therefore cannot bypass the same ±45° boundary.

The user reports no mechanical interference through ±45° Pitch. The software boundary still limits commands without sensing position. After reflashing, check the changed pulse scale at 0°, then ±5°, and inspect direction and platform angle before moving farther.

## Pitch time-based response

Pitch target changes schedule a nonblocking linear trajectory. The main loop calls `PitchAxis_Process(now_ms)` every pass; Servo output is updated no more often than every 20 ms. The default target-to-target response time is 1000 ms, adjustable from 200 to 5000 ms. These are initial tuning values, not measured mechanical specifications.

The duration is latched when each target arrives. Changing the configured response time during a move affects the next target only. A retarget starts from the latest Pitch command already sent to the Servo, so it does not jump back to an older trajectory start. The implementation uses integer arithmetic and does not delay the main loop.

Ozone exposes separate fields for requested Pitch, current software command, mapped Servo angle, PWM pulse, movement state, configured/active response time and elapsed trajectory time. `measured_mdeg` remains `INT32_MIN` with `measurement_valid=0`; a smoothed command is not a sensor measurement.

Pitch uses position commands plus software limits and time-based smoothing. No external Pitch PID is planned while the platform has no Pitch feedback sensor and the Servo is itself a position actuator.

## Yaw and feedback boundary

### No-slip-ring cable-wrap limit

The Pitch wiring follows the Yaw mechanism and there is no slip ring. Yaw is a
finite linear coordinate centered on the manually established cable-neutral
position; it is not a modulo-360 angle:

```text
-180°                 0°                 +180°
  |--------------------|--------------------|
                       ^
                cable neutral
```

The non-disableable command limits are `-180000..+180000 mdeg`; the matching
`-800..+800 PUL` bounds are derived at compile time from those angles and
`YAW_AXIS_PULSES_PER_REV`. Both the requested angle and its quantized
angle/pulse endpoint are checked. Commands are rejected at the boundary; they
are never clamped. A move from +170° to -170° follows the signed linear delta
of about -340°, not a +20° shortest path. No modulo or shortest-path wrapping
is used.

`YAW_AXIS_PULSES_PER_REV` means the configured PUL count for one full
revolution of the Yaw output axis/platform, not merely the motor shaft. The
current value, 1600, is an assumption based on the reported 1.8° motor, the
selected 8-microstep DIP row, and an assumed 1:1 motor-to-platform coupling.
The actual DIP lever positions, transmission ratio, and pulse-to-platform
angle are not yet verified; `YAW_AXIS_SCALE_VERIFIED` therefore defaults to
0. Until those checks pass, ±800 PUL and ±180° are software command bounds,
not verified physical travel limits.

The Yaw-specific rate range is initially limited to 20–500 PUL/s. This
500 PUL/s ceiling is a conservative bring-up setting, separate from the
TB6600 timer/electrical range; it is not a motor rating or final performance
target. Under the current 1600 PUL/output-revolution assumption it is
nominally 18.75 rpm. Raise it only after measured motion is reliable and a
review updates the limit.

At boot, Yaw reference is `INVALID`, even if the firmware pulse counter is
zero. The operator must place the mechanism at the natural cable route with
the Stepper disabled, then issue `DEBUG_CMD_SET_YAW_ZERO`. `SET_YAW_ZERO` is
rejected while enabled, moving, stopping or faulted. `STOP` preserves a valid
reference. An idle `DISABLE` invalidates it immediately. If disable is
requested while the axis is running, the reference and final commanded angle
remain available through `STOPPING`, then become invalid when the Stepper
reaches `DISABLED`; a fault also invalidates reference. Re-enable alone does
not restore it; set cable zero again while disabled before the next move.

`STEPPER MOVE` is a Debug bench relative-pulse command, but it also passes
through `YawAxis_MoveRelativePulses()` and is checked against the same
reference and pulse limits. Debug and Protocol entry paths cannot bypass the
Yaw cable boundary.

### Open-loop feedback boundary

For the reported 1.8° motor and the selected TB6600 8-microstep row:

```text
200 full steps/motor revolution × 8 = 1600 PUL/motor revolution
assumed 1:1 coupling → 1600 PUL/Yaw-output revolution
360000 mdeg / 1600 = 225 mdeg/PUL (under that assumption)
```

Angle requests round to the nearest signed pulse (half steps away from zero);
requested and quantized targets are both retained. Absolute Yaw requests
require a manually established cable-neutral zero and an enabled, idle
Stepper. Firmware pulse counts cannot detect missed steps, motor stall,
mechanical slip, incorrect DIP settings, a transmission-ratio mismatch, or
hand movement while unpowered. The cable limit is a software command guard,
not a sensor or mechanical stop. Verify the scale with small, low-speed
movements before approaching the configured endpoints; do not begin with a
full revolution.

Pitch and Yaw measured fields remain invalid until real sensors are added. Do not copy commanded values into measured fields. Yaw PID remains out of scope until encoder, IMU or other valid yaw feedback is available; this round adds no PID, sensor, homing, limit switch, DMA acceleration or RTOS.

## Configuration and verification

Reviewable control settings and the public Ozone interface are centralized in [control_debug_config.h](../Config/control_debug_config.h). `project_config.h` retains transport and peripheral settings. Limits and response tuning are not written into writable Ozone tuning fields; runtime tuning is limited to Pitch response time and is range checked.

Host tests cover horizontal zero, Pitch limits, Yaw cable angle/pulse
endpoints and rejects, Yaw scale constants, 20–500 PUL/s policy, linear
+170° to -170° movement, relative-pulse Protocol and Ozone entry paths,
reference invalidation after completed disable and fault, reference retention
during asynchronous stopping, telemetry validity, 20 ms update cadence,
response-time range/latching, Release gates and snapshot consistency. These
checks establish software behavior only. DIP state, transmission ratio,
pulse-to-platform scale, Servo direction/travel, cable-neutral placement, and
physical safety remain pending bench verification.

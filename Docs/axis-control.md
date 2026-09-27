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
| Pitch `0 mdeg` | Platform horizontal |
| Servo `130000 mdeg` | Mechanically observed near horizontal on the installed linkage |
| Pitch `+10000 mdeg` | Platform target 10° in the configured positive direction |

`PITCH_LEVEL_SERVO_MDEG` and `PITCH_SERVO_DIRECTION_SIGN` in [control_debug_config.h](../Config/control_debug_config.h) define the conversion:

```text
servo target = pitch horizontal anchor + direction sign × Pitch target
```

The current anchor is a mechanically observed initial value, not a precision Servo/linkage calibration. `PITCH_LEVEL_SERVO_MDEG` retains full millidegree precision, so a later measured anchor such as 130250 or 130500 mdeg is not truncated during Servo center math. `SERVO_CENTER_ANGLE_DEG` is display/legacy integer-degree data only; control and pulse conversion use `SERVO_CENTER_ANGLE_MDEG`. The PWM output remains quantized to whole microseconds. Positive direction is the current software selection; verify it with a secured mechanism before relying on it.

## Pitch limits and actuation path

Pitch commands are signed integer millidegrees. The enforced range is always:

```text
-30000 mdeg ≤ Pitch target ≤ +30000 mdeg
```

There is no runtime or build option to disable this Pitch limit. Compile-time checks ensure both configured endpoints map within the Servo logical range. With the current anchor and positive direction, the endpoints map to Servo 100° and 160°.

All application angle requests use `PitchAxis_SetTargetMilliDeg()`. Protocol, Ozone mailbox and future application callers pass through PitchAxis validation before Servo PWM changes. Raw absolute Servo angle and pulse commands are bench-only, compiled out of Release, and checked by converting the requested output back to Pitch coordinates before applying it. Raw pulse input therefore cannot bypass the same ±30° boundary.

The software boundary limits commands; it is not a mechanical stop, sensor, or proof that the physical linkage can safely travel the entire configured range. Start hardware checks at 0°, then ±5°, and inspect clearance and direction before trying larger values.

## Pitch time-based response

Pitch target changes schedule a nonblocking linear trajectory. The main loop calls `PitchAxis_Process(now_ms)` every pass; Servo output is updated no more often than every 20 ms. The default target-to-target response time is 1000 ms, adjustable from 200 to 5000 ms. These are initial tuning values, not measured mechanical specifications.

The duration is latched when each target arrives. Changing the configured response time during a move affects the next target only. A retarget starts from the latest Pitch command already sent to the Servo, so it does not jump back to an older trajectory start. The implementation uses integer arithmetic and does not delay the main loop.

Ozone exposes separate fields for requested Pitch, current software command, mapped Servo angle, PWM pulse, movement state, configured/active response time and elapsed trajectory time. `measured_mdeg` remains `INT32_MIN` with `measurement_valid=0`; a smoothed command is not a sensor measurement.

Pitch uses position commands plus software limits and time-based smoothing. No external Pitch PID is planned while the platform has no Pitch feedback sensor and the Servo is itself a position actuator.

## Yaw and feedback boundary

Yaw remains open loop. For the reported 1.8° motor and the selected TB6600 8-microstep row:

```text
200 full steps/rev × 8 = 1600 PUL/rev
360000 mdeg / 1600 = 225 mdeg/PUL
```

Angle requests round to the nearest signed pulse (half steps away from zero); requested and quantized targets are both retained. Absolute Yaw requests require a manually established zero and an enabled, idle Stepper. Firmware pulse counts cannot detect lost steps, shaft motion while unpowered, or hand movement.

Pitch and Yaw measured fields remain invalid until real sensors are added. Do not copy commanded values into measured fields. Yaw PID remains out of scope until encoder, IMU or other valid yaw feedback is available; this round adds no PID, sensor, homing, limit switch, DMA acceleration or RTOS.

## Configuration and verification

Reviewable control settings and the public Ozone interface are centralized in [control_debug_config.h](../Config/control_debug_config.h). `project_config.h` retains transport and peripheral settings. Limits and response tuning are not written into writable Ozone tuning fields; runtime tuning is limited to Pitch response time and is range checked.

Host tests cover horizontal zero, fixed limit endpoints and rejects, raw-angle/raw-pulse limit enforcement, protocol and Ozone entry paths, 20 ms update cadence, linear midpoint, retarget continuity, response-time range/latching, Release gates and snapshot consistency. These checks establish software behavior only. Servo direction/travel, pulse calibration and physical safety remain pending bench verification.

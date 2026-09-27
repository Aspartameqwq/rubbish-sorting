# Pitch/Yaw axis control foundation

## Scope and evidence

This round adds axis-level command and state APIs. It does not claim calibrated or measured mechanical angles. The Servo model and its safe travel are not yet confirmed, the reported mechanism has no Pitch/Yaw sensor wired to the STM32, and the TB6600 electrical path has not been measured. Current software limits are therefore disabled and must not be described as safety limits.

The layers are:

```text
PitchAxis → Servo BSP → TIM2_CH1 / PA0
YawAxis   → Stepper  → TB6600 BSP → TIM3_CH1 / PA6, PB12, PB13
```

Application angle commands should use `PitchAxis_SetTargetMilliDeg()` and `YawAxis_SetTargetMilliDeg()`. The legacy UART `STEPPER ...` commands remain raw bench controls and can bypass YawAxis reference/soft-limit policy.

## Units and state names

Control APIs use signed `int32_t` millidegrees (`mdeg`): 1° = 1000 mdeg. State keeps three distinct concepts:

| Field | Meaning | Current availability |
|---|---|---|
| `target_mdeg` | Requested logical angle | Available after an axis command |
| `commanded_mdeg` | Logical angle estimated from the command sent to the actuator | Available when the actuator mapping/reference is valid |
| `measured_mdeg` | Sensor measurement of the real mechanical axis | Unavailable; `INT32_MIN`, `measurement_valid=0` |

`commanded_mdeg` is not a physical position measurement. Servo pulse quantization can make the commanded estimate differ slightly from the requested target. Yaw command position comes from completed MCU pulse events; lost motor steps, manual shaft movement or power loss are not observable.

## Pitch

PitchAxis uses the Servo’s configured logical range of 0–270°. The project-selected nominal horizontal reference is **130°**, which maps to the existing center PWM pulse of **1500 µs**. The configured endpoints remain 0°→1400 µs and 270°→1600 µs; the generic BSP uses two linear segments around the center anchor, then reports the estimate inverse-mapped from the integer-microsecond pulse.

This is the software coordinate convention the user selected. It does not establish the actual horn/platform orientation or safe mechanical endpoints. `SERVO_CALIBRATION_VALID` remains 0 until the actual Servo model, linkage and safe pulse range are physically checked. The raw calibration API accepts only the configured 1400–1600 µs window; it sets raw-pulse mode and invalidates target/commanded-angle telemetry.

Current config:

```c
SERVO_CENTER_ANGLE_DEG = 130
SERVO_CENTER_PULSE_US = 1500
SERVO_CALIBRATION_VALID = 0
```

## Yaw

The selected TB6600 label row is 8 microsteps for the reported 1.8° motor:

```text
200 full steps/rev × 8 = 1600 PUL/rev
360000 mdeg / 1600 = 225 mdeg/PUL
```

Pulse-to-angle conversion is exact in integer arithmetic. Angle-to-pulse conversion rounds to the nearest signed integer pulse, with exact half steps rounded away from zero. Requested and quantized targets are both retained; for example `1000 mdeg` quantizes to 4 pulses, or `900 mdeg`. The actuator cannot represent every millidegree target.

YawAxis does not treat reset-time Stepper position as mechanical zero. `YawAxis_SetCurrentPositionAsZero()` stores the current Stepper count as a separate `zero_offset_pulses`, without changing Stepper’s count. The caller must first physically align the mechanism to the chosen zero while it is safe and stationary. The reference state then becomes `YAW_REFERENCE_MANUAL`; homing and sensor reference states are reserved but not implemented.

Absolute Yaw targets require a valid reference and an enabled, idle Stepper. Frequency must remain within the configured initial 20–10,000 PUL/s range. Call `YawAxis_Enable()` before requesting angle motion. The relative logical angle is computed from `(Stepper commanded pulses - zero_offset_pulses) × 225 mdeg`.

## Software limits

`Config/project_config.h` defines the limit ranges and validity flags:

```c
PITCH_SOFT_LIMIT_VALID = 0
PITCH_SOFT_MIN_MDEG = 0
PITCH_SOFT_MAX_MDEG = 270000

YAW_SOFT_LIMIT_VALID = 0
YAW_SOFT_MIN_MDEG = -180000
YAW_SOFT_MAX_MDEG = 180000
```

These are placeholders only. With a validity flag set to 0, the debug snapshot reports the limit as disabled and the range is not treated as calibrated protection. After actual mechanism measurement and review, setting a flag to 1 enables rejection: Pitch rejects requested logical targets outside its range; Yaw rejects requested or quantized pulse targets outside its range. Rejections are counted. Commands are never clamped. Yaw still requires a valid reference before an absolute limit can be evaluated.

The counters and most recent status are exposed in `g_debug_state`; status and count update on axis command attempts. Raw `SERVO_US` and `STEPPER MOVE` are bench interfaces and must not be used as production commands after limits are enabled.

## Feedback and PID roadmap

No external PID is implemented or connected to an actuator. A command estimate must never be copied into `measured_mdeg` to simulate feedback. The Servo model itself is not confirmed, and no sensor output is currently connected to this firmware.

Future work should proceed in this order:

1. Add a real Pitch/Yaw sensor interface and validate units, orientation, range, update rate and faults.
2. Populate `measured_mdeg` only from that sensor and set `measurement_valid` only for valid samples.
3. Add a hardware-independent, integer or appropriately reviewed PID math module with host coverage.
4. Connect PID output to PitchAxis/YawAxis only after feedback validity, output bounds, saturation and stop behavior are defined.
5. Tune gains with the mechanism secured; add integrator limits/anti-windup and document measured results.

Pitch PID would mean platform target minus a sensor-measured platform Pitch, with its output setting a Servo target. It must not compare a Servo command against itself. Yaw PID likewise needs IMU, encoder or another real yaw measurement; Stepper pulse count is not feedback.

## Host verification

Host tests cover angle conversion, signed rounding, manual zero offset, requested/quantized targets, Servo mapping and raw-mode invalidation. Separate host configurations compile with software limits disabled/enabled and with Debug command injection enabled/disabled. These are software checks; hardware calibration and motor/Servo behavior remain pending.

# Stepper control and motion profile

## Current control model

`Motion/stepper.c` implements one-axis open-loop finite moves using the public TB6600 BSP. It never accesses HAL, GPIO, TIM3 registers, UART or Protocol internals. Positive signed steps select configured forward direction; negative steps select reverse. Magnitude is computed using `int64_t`, so `INT32_MIN` is valid. The requested endpoint is checked against the signed 32-bit commanded-position range before the move starts.

`Stepper_GetCommandedPosition()` represents firmware-counted completed PUL compare events. It is not actual shaft position: no encoder, feedback, homing or closed-loop correction exists. Lost motor steps, a truncated emergency-stop pulse, driver disable, power loss or manual shaft movement can make physical position differ from the counter.

The user reports a motor step angle of 1.8° and 1.5 A current. At full step, 1.8° corresponds to 200 PUL pulses per revolution. With the TB6600 module set to microstep factor `M`, nominally use `200 × M` pulses per motor revolution, assuming no gearbox. `Stepper_MoveSteps()` and `STEPPER MOVE` count PUL pulses, while the frequency is PUL pulses per second; neither value is converted to full steps or degrees by the firmware. The microstep DIP setting must be known before translating a requested rotation into pulses.

## State machine

```text
UNINITIALIZED → DISABLED ⇄ IDLE → DIR_SETUP → RUNNING → IDLE
                                      RUNNING → STOPPING → IDLE
                                                        └→ DISABLED
Any driver/timer failure → FAULT
```

- `ENABLE` asserts the configured ENA active level; `DISABLE` deasserts it.
- `MOVE` configures a fixed pulse frequency and direction, then waits `STEPPER_DIRECTION_SETUP_MS` using a wrap-safe tick delta. It does not block with `HAL_Delay()`.
- `STOP` and a disable during motion request a stop at the next complete pulse boundary. The last pulse is counted before the controller returns idle or disables ENA.
- `EmergencyStop` stops PWM immediately from main context. It can cut short the pulse currently active; the module may or may not recognize such a pulse. It is not exposed as a text command.
- A new move is rejected while direction setup, running or stopping is active. A zero-step move with a valid frequency is a successful no-op.

## Timer waveform and finite step count

TIM3 timer clock is 72 MHz. Generated PSC 71 gives a 1 MHz counter (1 µs/count). For requested frequency `f`:

```text
period_counts = 1,000,000 / f
ARR           = period_counts - 1
CCR           = TB6600_PULSE_HIGH_US (default 10)
```

The 16-bit ARR and `CCR < ARR` conditions are checked before registers are updated. Default software rate limits are 20–10,000 steps/s; they are initial limits, not validated driver performance. PWM1 creates a fixed active pulse and variable low interval. PUL is produced entirely by TIM3 hardware.

The finite move count is based on TIM3 CH1 compare/pulse-finished events, not Update IRQ. In PWM1 the compare event marks the end of the active PUL width. The callback increments one completed pulse; on pulse N it immediately stops PWM before the next timer period can begin. Therefore the firmware requests and counts exactly N complete active pulse widths for an uninterrupted normal move, with no start-update event counted as a step and no N+1 pulse. This is the user-selected resolution of the earlier Update-vs-compare design choice.

The interrupt callback performs the event count and terminal stop only. Stepper position, remaining count, and state changes happen in `Stepper_Process()` on the main loop. At the configured maximum rate the period is 100 µs and the inactive interval after the 10 µs pulse is 90 µs. The TIM3 compare IRQ must be serviced before the next rising edge (within that 90 µs interval) to prevent an extra pulse; servicing each compare in under one full 100 µs period is also required to avoid collapsed pending compare flags. Keep the generated TIM3 IRQ priority at its current highest setting and keep global interrupt-masked sections short.

## Commanded position and abort semantics

Position increments/decrements only when the completed-pulse count is consumed on the main loop. It is an open-loop firmware count, not a sensor measurement. Graceful `STOP` and `DISABLE` wait for the next falling/compare edge, so they do not intentionally truncate a normal pulse. The non-protocol emergency API stops immediately; if it interrupts an active pulse, only compare-completed pulses are added to commanded position, while actual driver recognition cannot be established without the module's electrical threshold and hardware verification.

## Profile foundation

`Motion/stepper_profile.c` is pure C integer math and can build without HAL. Given total steps, start frequency, maximum frequency and acceleration in steps/s², it returns a symmetric frequency for each zero-based step index. It uses the discrete kinematic relation `v² = v0² + 2 a s`, integer square root, and caps the peak at the configured maximum. Short moves form a triangular profile; long enough moves reach the maximum and form a trapezoid. The API rejects invalid input and out-of-range step indexes. It produces a mathematical sequence only; the current constant-speed Stepper API does not yet apply it to timer ARR updates.

No floating point, dynamic allocation, S-curve or multi-axis interpolation is used. Tests cover one-, two-, short- and long-step profiles, symmetry, frequency bounds and extreme `uint32_t` inputs.

## Future profile DMA design

When acceleration is integrated, preserve the public Stepper interface and constant-speed backend. TIM3 Update may request DMA1 Channel 3 to stream ARR values while TIM3_CH1 continues hardware PWM. DMA1 Channel 5 remains USART1_RX, Channel 4 is optional USART1_TX, and Channel 6 remains unused for TIM3_CH1. The future backend needs separate timing/underrun/finite-count tests and must not change direction during a running profile. No such DMA request or backend is enabled now.

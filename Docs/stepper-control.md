# Stepper control and motion profile

## Current control model

`Motion/stepper.c` implements one-axis open-loop finite moves using the public TB6600 BSP. YawAxis owns angle-based scheduling and wraps `Stepper_Process()`. UART `STEPPER ...` commands are Debug bench controls, but every Yaw action now routes through YawAxis; relative Stepper pulses cannot bypass the mandatory cable range. Stepper itself never accesses HAL, GPIO, TIM3 registers, UART or Protocol internals. Positive signed steps select configured forward direction; negative steps select reverse. Magnitude is computed using `int64_t`, so `INT32_MIN` is parsed safely then rejected by YawAxis when it would exceed the cable range. The Stepper checks the signed 32-bit commanded-position range before a move starts.

`Stepper_GetCommandedPosition()` represents firmware-counted completed PUL compare events. It is not actual shaft position: no encoder, feedback, homing or closed-loop correction exists. Lost motor steps, a truncated emergency-stop pulse, driver disable, power loss or manual shaft movement can make physical position differ from the counter.

The user reports a motor step angle of 1.8° and approximately 1.5 A current. The project selects 8 microsteps from the pictured module label: 200 full steps/revolution × 8 = 1600 PUL pulses/revolution, nominally 0.225° per pulse with direct coupling. `Stepper_MoveSteps()` and `STEPPER MOVE` count PUL pulses, while frequency is PUL pulses per second; the low-level Stepper module does not convert requested pulses into degrees. YawAxis provides the separate mdeg/pulse mapping and requires a manual zero before absolute angle commands. The selected DIPs and full wiring are recorded in [Docs/wiring.md](wiring.md); actual switch positions and motion remain PENDING verification.

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

TIM3 timer clock is 72 MHz. Generated PSC 71 gives a 1 MHz counter (1 µs/count). For requested PUL pulse frequency `f`:

```text
period_counts = 1,000,000 / f
ARR           = period_counts - 1
CCR           = TB6600_PULSE_HIGH_US (default 10)
```

The 16-bit ARR and `CCR < ARR` conditions are checked before registers are updated. Default software rate limits are 20–10,000 PUL pulses/s; they are initial limits, not validated driver performance. At the selected 1600 pulses/revolution, 10,000 pulses/s corresponds mathematically to 375 rev/min, but the motor may not reach that rate. PWM1 creates a fixed active pulse and variable low interval. PUL is produced entirely by TIM3 hardware.

The finite move count is based on TIM3 CH1 compare/pulse-finished events, not Update IRQ. In PWM1 the compare event marks the end of the active PUL width. The callback increments one completed pulse; on pulse N it immediately stops PWM before the next timer period can begin. Therefore the firmware requests and counts exactly N complete active pulse widths for an uninterrupted normal move, with no start-update event counted as a step and no N+1 pulse. This is the user-selected resolution of the earlier Update-vs-compare design choice.

The interrupt callback performs the event count and terminal stop only. Stepper position, remaining count, and state changes happen in `Stepper_Process()` on the main loop. At the configured maximum rate the period is 100 µs and the inactive interval after the 10 µs pulse is 90 µs. The TIM3 compare IRQ must be serviced before the next rising edge (within that 90 µs interval) to prevent an extra pulse; servicing each compare in under one full 100 µs period is also required to avoid collapsed pending compare flags. Keep the generated TIM3 IRQ priority at its current highest setting and keep global interrupt-masked sections short.

## Commanded position and abort semantics

Position increments/decrements only when the completed-pulse count is consumed on the main loop. It is an open-loop firmware count, not a sensor measurement. Graceful `STOP` and `DISABLE` wait for the next falling/compare edge, so they do not intentionally truncate a normal pulse. The non-protocol emergency API stops immediately; if it interrupts an active pulse, only compare-completed pulses are added to commanded position, while actual driver recognition cannot be established without the module's electrical threshold and hardware verification.

## Profile foundation

`Motion/stepper_profile.c` is pure C integer math and can build without HAL. Given total steps, start frequency, maximum frequency and acceleration in steps/s², it returns a symmetric frequency for each zero-based step index. It uses the discrete kinematic relation `v² = v0² + 2 a s`, integer square root, and caps the peak at the configured maximum. Short moves form a triangular profile; long enough moves reach the maximum and form a trapezoid. The API rejects invalid input and out-of-range step indexes. It produces a mathematical sequence only; the current constant-speed Stepper API does not yet apply it to timer ARR updates.

No floating point, dynamic allocation, S-curve or multi-axis interpolation is used. Tests cover one-, two-, short- and long-step profiles, symmetry, frequency bounds and extreme `uint32_t` inputs.

## Yaw cable safety boundary

Yaw has no slip ring, so the Pitch cable can twist with Yaw rotation. Yaw uses a finite linear coordinate around an operator-established cable-neutral zero. The non-disableable range is `-180000..+180000 mdeg` and `-800..+800 PUL` for the selected 1600 PUL/rev mapping. Absolute angle targets are not modulo-360 or shortest-path wrapped; +170° to -170° means about -340° of commanded travel and is rejected if an endpoint would leave the range.

Boot reference is invalid. `YawAxis_SetCurrentPositionAsZero()` is accepted only while the Stepper state is `DISABLED`; STOP preserves reference, and YawAxis DISABLE invalidates it because the shaft can be moved by hand without feedback. `YawAxis_MoveRelativePulses()` checks the final position relative to cable zero with a wide intermediate before scheduling `Stepper_MoveSteps()`. UART `STEPPER MOVE` uses this wrapper; no protocol or Ozone movement command may call the low-level Stepper move directly. See [axis-control.md](axis-control.md) and [debugging.md](debugging.md) for the reference workflow and telemetry.

## Future profile DMA design

When acceleration is integrated, preserve the public Stepper interface and constant-speed backend. TIM3 Update may request DMA1 Channel 3 to stream ARR values while TIM3_CH1 continues hardware PWM. DMA1 Channel 5 remains USART1_RX, Channel 4 is optional USART1_TX, and Channel 6 remains unused for TIM3_CH1. The future backend needs separate timing/underrun/finite-count tests and must not change direction during a running profile. No such DMA request or backend is enabled now.

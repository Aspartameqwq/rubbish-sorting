# Architecture and module boundaries

## Status

The project now separates the mechanical Pitch and Yaw axes from their actuator drivers. PitchAxis maps logical millidegrees through the Servo BSP; YawAxis maps logical millidegrees through the open-loop Stepper/TB6600 path and an explicit manual reference. Ozone telemetry and command injection are provided by Diagnostics. No angle sensor or closed-loop PID output is implemented. Hardware calibration and electrical behavior remain pending; see [axis control](axis-control.md), [debugging](debugging.md), and [development](development.md).

## Layers and dependencies

```text
Core/main
   ↓
  App ──────────────→ Protocol ─────→ HC-04 BSP ─────→ USART1/DMA
   │                    │
   │                    ├─ axis commands → Control
   │                    └─ raw bench Stepper commands → Motion/Stepper
   │
   ├───────────────→ Diagnostics ───→ public Control/actuator getters
   │                    └──────────── debugger mailbox → Control APIs
   │
   ├───────────────→ PitchAxis ─────→ Servo BSP ──────→ TIM2_CH1
   └───────────────→ YawAxis ───────→ Stepper ────────→ TB6600 BSP ─→ TIM3_CH1/GPIO
                                      Motion profile (pure integer math)
```

| Layer | Owns | Must not own |
|---|---|---|
| Core | CubeMX startup, clock/peripheral setup, HAL handles and IRQ dispatch | Command parsing, axis policy or actuator logic |
| App | Initialization order, health bitmask and repeated nonblocking processing | Peripheral register access or protocol internals |
| Protocol | Bounded line assembly, strict parsing and ASCII replies | HAL calls, GPIO/TIM/DMA handles or transport state |
| Control/PitchAxis | Pitch target, logical range/soft-limit checks and Servo command mapping | TIM handles, UART, Servo-driver internals or sensor claims |
| Control/YawAxis | Yaw target, reference offset, pulse/angle conversion, limit checks and Stepper scheduling | HAL/GPIO/TIM registers, UART or physical position feedback |
| Motion/Stepper | Requested pulse moves, direction setup, pulse progress and commanded pulse count | HAL, GPIO/TIM handles or command parsing |
| Motion/profile | HAL-free integer trapezoidal/triangular frequency sequence | Hardware state, allocation or floating point |
| Diagnostics | Ozone mirror snapshot and debugger request mailbox | Control decisions from telemetry, direct HAL/register access or private state |
| BSP | Servo PWM; HC-04 UART/DMA; TB6600 direction/enable/PWM | Axis strategy, command grammar or application workflow |
| Config | Shared hardware assumptions, ranges, limits, timings and buffer sizes | Runtime state or hidden initialization |

`App` initializes Servo, HC-04, TB6600, Stepper, then both axes. Its processing loop advances HC-04 transport, calls `YawAxis_Process()` (the only App-facing path to `Stepper_Process()`), processes Protocol, and services Diagnostics. Pitch currently has no asynchronous processing step.

Angle-level Servo commands go through PitchAxis. `SERVO_US` and `STEPPER ...` low-level protocol commands remain bench/debug interfaces; `STEPPER MOVE` intentionally bypasses YawAxis reference and software-limit checks. They must not be reused as production/K230 angle commands. Future application commands should call PitchAxis/YawAxis.

## Data flow

```text
HC-04 → USART1 → DMA1_CH5 circular buffer → HC04_Process()
      → bounded Protocol parser → PitchAxis → Servo → TIM2_CH1 / PA0
                              └→ raw bench Stepper API → Motion/Stepper → TB6600 → TIM3_CH1 / PA6
      ← HC04_Send() ← bounded ASCII response

Ozone Watch → g_debug_command mailbox → Debug_Process() → PitchAxis / YawAxis APIs
Ozone Watch ← g_debug_state snapshot ← public axis/BSP getters
```

The HC-04 DMA buffer is the only transport byte buffer. Protocol owns one fixed command line. DMA overwrite invalidates a partial command and resynchronizes at LF. Each `Protocol_Process()` call consumes at most `PROTOCOL_MAX_BYTES_PER_PROCESS` bytes. Diagnostics updates a public-symbol snapshot every 20 ms in main context; control mailbox requests are checked each main-loop iteration.

## Angle and feedback boundary

Pitch target and commanded values use mdeg. Pitch commanded angle is an estimate derived from the configured angle-to-pulse map and PWM quantization. Yaw commanded angle is derived from completed PUL counts relative to the manual zero offset. Neither is a measured mechanical angle. Both measured fields use `INT32_MIN` and `measurement_valid=0` until a real sensor path is implemented.

Software limits are disabled by default because the values have not been calibrated to the mechanism. When enabled, axis APIs reject out-of-range requests; they do not clamp. Absolute Yaw commands require a valid reference. Details and the future PID boundary are in [axis-control.md](axis-control.md).

## Interrupt boundary

The USART/DMA callbacks record DMA boundaries and UART/DMA error state. The TIM3 CH1 compare callback counts a completed active PUL width at the PWM1 compare/falling edge. It performs the minimum terminal hardware cutoff on the final requested pulse (or a graceful stop request) so another period cannot begin. Position changes and Stepper state transitions remain in `YawAxis_Process()` on the main loop. No parser, string formatting, UART transmit, direction change or long calculation runs in the callback.

The compare-edge count intentionally differs from the initial Update IRQ proposal: counting at period wrap could allow the next pulse to begin before the main loop disables PWM. Counting the active pulse's falling edge and cutting off at that edge allows the finite move to emit exactly N complete active pulse widths.

## Peripheral ownership

- `servo.c` alone owns TIM2_CH1 compare and start/stop; Servo PWM has no DMA. Its generic mapping anchors configured center angle to center pulse; PitchAxis selects 130° as the nominal horizontal logical reference.
- `hc04.c` owns USART1 transport and RX DMA1_CH5.
- `tb6600.c` alone owns TIM3_CH1, PB12 and PB13.
- `stepper.c` owns open-loop pulse and commanded-position state, but no peripheral handles. YawAxis owns its scheduling interface.
- `protocol.c` sends all replies through `HC04_Send()`.
- Diagnostics reads only public getters and receives time/health as arguments. It never calls HAL or reads its own telemetry back into control.
- DMA1_CH3 is reserved for a future TIM3_UP profile backend; DMA1_CH4 is optional USART1_TX; DMA1_CH5 remains USART1_RX; TIM3_CH1's DMA1_CH6 mapping is available but unused.

## Source and generation ownership

```text
Core/                    CubeMX generated startup and peripherals
Config/project_config.h  shared hardware, axis, and limit defaults
BSP/Inc + BSP/Src        Servo, HC04, TB6600, timing conversion
Motion/Inc + Motion/Src  Stepper and pure integer profile
Control/Inc + Control/Src PitchAxis and YawAxis
Diagnostics/Inc + Diagnostics/Src Ozone state and command mailbox
Protocol/Inc + Protocol/Src
App/Inc + App/Src
tests/host/              native C tests and hardware stubs
```

CubeMX owns `.ioc`, `Core/*` and the generated `cmake/stm32cubemx/*` directory. The root build intentionally does not include the generated CMake file because CubeMX writes machine-local package paths there. Package resolution lives in `cmake/stm32cube_f1.cmake`; review/synchronize generated Core source additions in root `CMakeLists.txt` after regeneration.

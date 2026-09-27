# Architecture and module boundaries

## Status

The project separates Pitch and Yaw coordinates from their actuator drivers. PitchAxis owns the relative-to-horizontal target, hard ±45° command range and Servo trajectory. YawAxis owns angle/pulse conversion, manual cable-neutral reference, mandatory ±180°/±800 PUL limits and Stepper scheduling; its position remains open loop. `g_control_debug` exposes Ozone telemetry, tuning and a single-request command mailbox. No angle sensor or PID actuator output is implemented. Hardware behavior remains pending; see [axis control](axis-control.md), [debugging](debugging.md), and [development](development.md).

## Layers and dependencies

```text
Core/main
   ↓
  App ───────────────→ Protocol ─────→ HC-04 BSP ─────→ USART1/DMA
   │                      ├─ PITCH → PitchAxis
   │                      └─ gated bench commands → PitchAxis / Motion
   ├──────────────────→ SystemTime → HAL_GetTick()
   ├──────────────────→ PitchAxis → Servo BSP → TIM2_CH1
   ├──────────────────→ YawAxis → Stepper → TB6600 BSP → TIM3_CH1/GPIO
   └──────────────────→ Diagnostics → g_control_debug
                                  └─ mailbox → PitchAxis / YawAxis APIs
```

| Layer | Owns | Must not own |
|---|---|---|
| Core | CubeMX startup, clock/peripheral setup, HAL handles and IRQ dispatch | Command parsing, axis policy or actuator logic |
| App | Initialization order, health flags, shared tick snapshot and nonblocking scheduling | Peripheral register access or protocol internals |
| Protocol | Bounded line assembly, strict parsing and ASCII replies | HAL calls, GPIO/TIM/DMA handles or transport state |
| Control/PitchAxis | Relative Pitch target, mandatory range checks, trajectory and Servo mapping | TIM handles, UART, Servo-driver internals or sensor claims |
| Control/YawAxis | Yaw target, cable-neutral reference offset, hard cable limits, pulse/angle conversion and Stepper scheduling | HAL/GPIO/TIM registers, UART or physical position feedback |
| Motion/Stepper | Requested pulse moves, direction setup, pulse progress and commanded pulse count | HAL, GPIO/TIM handles or command parsing |
| Motion/profile | HAL-free integer trapezoidal/triangular frequency sequence | Hardware state, allocation or floating point |
| Diagnostics | Stable-sequence telemetry snapshot, Ozone tuning and single-request mailbox | Control decisions from telemetry, direct HAL/register access or private state |
| BSP | Servo PWM; HC-04 UART/DMA; TB6600 direction/enable/PWM | Axis strategy, command grammar or application workflow |
| Config | Hardware assumptions, axis calibration/ranges, trajectory defaults and Debug interface | Runtime axis state or hidden initialization |

`App` initializes Servo, HC-04, TB6600, Stepper and then both axes. Each `App_Process()` call samples `SystemTime_GetMs()` once, advances HC-04, calls `PitchAxis_Process(now_ms)` and `YawAxis_Process()`, handles Protocol, then Diagnostics. Pitch changes are time based and do not block the loop.

Normal UART Pitch control is `PITCH <signed-mdeg>` and goes through PitchAxis. Absolute Servo angle and raw PWM are Debug-only bench paths; raw Pitch paths are converted back into Pitch coordinates and checked against the same hard range. Debug UART Stepper actions pass through YawAxis: `STEPPER MOVE` uses the relative-pulse cable/rate guard, `STEPPER STOP` preserves a valid cable reference, and `STEPPER DISABLE` invalidates it once shutdown completes (while retaining reference through `STOPPING`). No Protocol or Diagnostics movement path calls the low-level Stepper move directly.

## Data flow

```text
HC-04 → USART1 → DMA1_CH5 circular buffer → HC04_Process()
      → bounded Protocol parser → PitchAxis → 20 ms trajectory → Servo → TIM2_CH1 / PA0
      ← HC04_Send() ← bounded ASCII response

App → SystemTime_GetMs() → PitchAxis_Process(now_ms), Debug_Process(now_ms, health)
App → YawAxis_Process() → Motion/Stepper → TB6600 → TIM3_CH1 / PA6

Ozone Watch → g_control_debug.command/tuning → Debug_Process() → axis APIs
Ozone Watch ← g_control_debug.state + snapshot_seq ← public axis/BSP getters
```

The HC-04 DMA buffer is the only transport byte buffer. Protocol owns one fixed command line. DMA overwrite invalidates a partial command and resynchronizes at LF. Each `Protocol_Process()` call consumes at most `PROTOCOL_MAX_BYTES_PER_PROCESS` bytes. Diagnostics updates the snapshot every 20 ms in main context; the mailbox is checked each main-loop iteration. The snapshot sequence is odd during writes and even after a complete update.

## Angle and feedback boundary

Pitch 0° is the platform horizontal position observed at Servo 148° (approximately 1596 µs). The user confirms a 1:1 Servo-to-platform angle ratio and clearance for ±45° Pitch. Positive Pitch increases the Servo angle and tilted the platform backward during the bench check. The software rejects targets outside ±45°; its commanded value is not sensor feedback.

Yaw commanded angle comes from completed PUL counts relative to the manual zero offset. Neither axis has measured-angle feedback: both measured fields stay `INT32_MIN` and `measurement_valid=0` until sensor paths are implemented. Absolute Yaw requests require a valid manual reference. PID remains out of scope without valid feedback.

## Interrupt boundary

The USART/DMA callbacks record DMA boundaries and UART/DMA error state. The TIM3 CH1 compare callback counts a completed active PUL width at the PWM1 compare/falling edge. It performs the minimum terminal hardware cutoff on the final requested pulse (or a graceful stop request) so another period cannot begin. Position changes and Stepper state transitions remain in `YawAxis_Process()` on the main loop. No parser, string formatting, UART transmit, direction change or long calculation runs in the callback.

Counting at the active pulse's falling edge allows a finite move to emit exactly N complete active pulse widths. Counting at period wrap could allow the next pulse to start before the main loop disables PWM.

## Peripheral ownership

- `servo.c` owns TIM2_CH1 compare and start/stop; PitchAxis is the application-level Servo actuator gateway.
- `hc04.c` owns USART1 transport and RX DMA1_CH5.
- `tb6600.c` owns TIM3_CH1, PB12 and PB13.
- `stepper.c` owns open-loop pulse and commanded-position state, but no peripheral handles. YawAxis owns the normal scheduling interface.
- `system_time.c` is the application-level millisecond clock adapter over `HAL_GetTick()`.
- `protocol.c` sends replies through `HC04_Send()` and invokes axis APIs; raw commands are compile gated.
- Diagnostics reads public getters, receives shared time/health arguments and never feeds telemetry back into control.
- DMA1_CH3 is reserved for a possible future TIM3_UP profile backend; DMA1_CH4 is optional USART1_TX; DMA1_CH5 remains USART1_RX; TIM3_CH1's DMA1_CH6 mapping is available but unused.

## Source and generation ownership

```text
Core/                    CubeMX generated startup and peripherals
Config/project_config.h  transport and peripheral defaults
Config/control_debug_config.h Pitch/Yaw review settings and Ozone interface
Platform/                shared system time adapter
BSP/Inc + BSP/Src        Servo, HC04, TB6600, timing conversion
Motion/Inc + Motion/Src  Stepper and pure integer profile
Control/Inc + Control/Src PitchAxis and YawAxis
Diagnostics/Inc + Diagnostics/Src Ozone state, tuning and mailbox
Protocol/Inc + Protocol/Src bounded ASCII command parsing
App/Inc + App/Src        initialization and main-loop scheduling
tests/host/              native C tests and hardware stubs
```

CubeMX owns `.ioc`, `Core/*` and the generated `cmake/stm32cubemx/*` directory. The root build intentionally does not include the generated CMake file because CubeMX may write machine-local package paths there. Package resolution lives in `cmake/stm32cube_f1.cmake`; review/synchronize generated Core source additions in root `CMakeLists.txt` after regeneration.

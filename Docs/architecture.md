# Architecture and module boundaries

## Status

Round 2 software is implemented: Servo and HC-04 fixes, TB6600 PWM/GPIO abstraction, fixed-frequency finite Stepper moves, and a separate integer profile foundation. Host/target build evidence is recorded in the development guide. Servo, HC-04 and TB6600 electrical/mechanical behavior remains unverified on hardware.

## Layers and dependencies

```text
Core/main
   ↓
  App ─────────→ Protocol ─────→ HC04 BSP ─────→ USART1/DMA
   │                  ├────────→ Servo BSP ────→ TIM2_CH1
   └─────────────────→ Stepper ─→ TB6600 BSP ──→ TIM3_CH1/GPIO
                             Motion profile (pure integer math)
```

| Layer | Owns | Must not own |
|---|---|---|
| Core | CubeMX startup, clock/peripheral setup, HAL handles and IRQ dispatch | Command parsing, motor policy or Servo logic |
| App | Initialization order, health bitmask and repeated nonblocking processing | Peripheral register access or protocol internals |
| Protocol | Bounded line assembly, strict parsing and ASCII replies | HAL calls, GPIO/TIM/DMA handles or transport state |
| Motion/Stepper | Requested motion, direction setup, pulse progress and commanded position | HAL, GPIO/TIM registers, UART or command parsing |
| Motion/profile | HAL-free integer trapezoidal/triangular frequency sequence | Hardware state, allocation or floating point |
| BSP | Servo PWM; HC-04 UART/DMA; TB6600 direction/enable/PWM | Higher-level motion, command grammar or application workflow |
| Config | Shared hardware assumptions, limits, timings and buffer sizes | Runtime state or hidden initialization |

`Protocol` calls public Servo, HC-04 and Stepper APIs. `Stepper` calls only public TB6600 BSP APIs. `tb6600.c` owns TIM3 and GPIO access. The profile module has no HAL dependency; this round exposes and tests it but does not yet drive a DMA/profile backend.

## Data flow

```text
HC-04 → USART1 → DMA1_CH5 circular buffer → HC04_Process()
      → bounded Protocol parser → Servo API → TIM2_CH1 / PA0
                              └→ Stepper API → TB6600 BSP → TIM3_CH1 / PA6
      ← HC04_Send() ← bounded ASCII response
```

The HC-04 DMA buffer is the only transport byte buffer. Protocol owns one fixed command line. DMA overwrite invalidates a partial command and resynchronizes at LF. Each `Protocol_Process()` call consumes at most `PROTOCOL_MAX_BYTES_PER_PROCESS` bytes.

## Main loop and interrupt boundary

`App_Process()` schedules HC-04 transport, Stepper state processing and Protocol processing without a delay. Direction setup uses a tick deadline rather than `HAL_Delay()`.

The USART/DMA callbacks record DMA boundaries and UART/DMA error state. The TIM3 CH1 compare callback counts a completed active PUL width at the PWM1 compare/falling edge. It performs the minimum terminal hardware cutoff on the final requested pulse (or a graceful stop request) so another period cannot begin. Position changes and Stepper state transitions remain in `Stepper_Process()` on the main loop. No parser, string formatting, UART transmit, direction change or long calculation runs in the callback.

The compare-edge count intentionally differs from the initial Update IRQ proposal: counting at period wrap could allow the next pulse to begin before the main loop disables PWM. Counting the active pulse's falling edge and cutting off at that edge allows the finite move to emit exactly N complete active pulse widths.

## Peripheral ownership

- `servo.c` alone owns TIM2_CH1 compare and start/stop; Servo PWM has no DMA.
- `hc04.c` owns USART1 transport and RX DMA1_CH5.
- `tb6600.c` alone owns TIM3_CH1, PB12 and PB13.
- `stepper.c` owns open-loop step and commanded-position state, but no peripheral handles.
- `protocol.c` sends all replies through `HC04_Send()`.
- DMA1_CH3 is reserved for a future TIM3_UP profile backend; DMA1_CH4 is optional USART1_TX; DMA1_CH5 remains USART1_RX; TIM3_CH1's DMA1_CH6 mapping is available but unused.

## Source and generation ownership

```text
Core/                    CubeMX generated startup and peripherals
Config/project_config.h
BSP/Inc + BSP/Src        Servo, HC04, TB6600, timing conversion
Motion/Inc + Motion/Src  Stepper and pure integer profile
Protocol/Inc + Protocol/Src
App/Inc + App/Src
tests/host/              Native C tests and hardware stubs
```

CubeMX owns `.ioc`, `Core/*` and the generated `cmake/stm32cubemx/*` directory. The root build intentionally does not include the generated CMake file because CubeMX writes machine-local package paths there. Package resolution lives in `cmake/stm32cube_f1.cmake`; review/synchronize generated Core source additions in root `CMakeLists.txt` after regeneration.

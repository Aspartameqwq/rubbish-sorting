# Architecture and module boundaries

## Status

The first-round CubeMX peripherals, project configuration, Servo/HC-04 BSP, command Protocol, and App scheduler are implemented. Physical Servo and HC-04 verification remains pending.

## Layers

| Layer | Owns | Must not own |
|---|---|---|
| Core | CubeMX startup, clock setup, HAL handles, interrupt dispatch, peripheral initialization | Servo policy, command parsing, product behavior |
| BSP | `servo` PWM operations; `hc04` UART/DMA transport, byte delivery, transport errors and counters | Text command parsing or application workflow |
| Protocol | Bounded line assembly, strict command parsing, range validation, response formatting | HAL calls, peripheral registers, DMA buffer ownership |
| App | Initialization order and repeated non-blocking module processing | Peripheral register access or protocol parsing internals |
| Config | Project-wide limits and hardware parameters used by firmware modules | Runtime state or hidden hardware initialization |

Dependency direction:

```text
main / Core → App → Protocol → public BSP APIs → HAL → STM32 hardware
                └────────────→ BSP APIs
Config → modules that consume project parameters
```

`BSP` must not depend on `App`. `Protocol` may call the public Servo and HC-04 APIs, but it must not access `htim2`, `huart1`, DMA handles, GPIO registers, or `HAL_UART_*` directly. `App` schedules both Protocol and BSP processing. Configuration values are shared through `Config/project_config.h`.

## First-round data flow

```text
HC-04 → USART1 → DMA1_CH5 circular buffer → HC04_Process()
      → Protocol line parser → Servo API → TIM2_CH1 PWM → PA0
      ← bounded USART1 response via HC-04 TX API
```

The 256-byte DMA buffer is the transport's only byte-stream buffer. The protocol owns one bounded 64-byte command-line buffer. Do not copy every received byte into another ring buffer. The DMA consumer must finish processing received data before DMA overwrites it; overrun invalidates the current line and forces resynchronization at the next newline.

## Interrupt and main-loop boundary

The USART/DMA HAL callback only records DMA half/full boundaries and UART/DMA error state. It must not parse strings, execute commands, move the servo, wait, or format replies. `HC04_Process()` samples the DMA remaining count in the main loop; the wrap counter makes the producer position monotonic across circular-buffer wraps. `App_Process()` schedules transport and protocol processing continuously. No blocking delay belongs in the normal receive path.

The STM32CubeF1 v1.8.7 HAL in the configured local package was inspected and provides `HAL_UARTEx_ReceiveToIdle_DMA()`, `HAL_UARTEx_RxEventCallback()`, and `HAL_UARTEx_GetRxEventType()`. The transport uses this package's API in circular mode. It counts DMA half-transfer/transfer-complete boundaries and samples NDTR in the main loop; HAL also enables IDLE events, but IDLE is not required for byte progress or used as the command delimiter. This avoids depending on IDLE alone, which is relevant to the device errata. LF is the protocol delimiter.

## Peripheral ownership

- `servo.c` is the only application module allowed to start TIM2_CH1 PWM or change its compare value. Servo PWM must not use DMA.
- `hc04.c` exclusively owns USART1 RX DMA state and HAL UART transport calls.
- `protocol.c` owns command grammar and response semantics, but sends responses through `HC04_Send()`.
- TIM2_CH1 and USART1_RX map to DMA1 Channel 5 on this MCU. Channel 5 is assigned to USART1_RX, so the Servo must not request DMA.
- TIM3_CH1/PA6, PB12, PB13, and DMA1 Channel 6 are reserved for later TB6600 work.

## Source layout

```text
Core/                 CubeMX generated startup, HAL and IRQ code
Config/project_config.h
BSP/Inc/servo.h       BSP/Src/servo.c
BSP/Inc/hc04.h        BSP/Src/hc04.c
Protocol/Inc/protocol.h
Protocol/Src/protocol.c
App/Inc/app.h         App/Src/app.c
Docs/
```

Every new `.c` file must be added explicitly to CMake. CubeMX owns `Core/*`, `*.ioc`, and `cmake/stm32cubemx/*`; reusable modules and project documentation stay outside those generated paths.

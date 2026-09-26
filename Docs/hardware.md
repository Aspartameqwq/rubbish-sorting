# Hardware and CubeMX configuration

## Evidence status

This table distinguishes checked-in/generated configuration from hardware values that still require confirmation.

| Item | Value | Status |
|---|---|---|
| MCU/package | STM32F103C8T6, LQFP48 | Confirmed by `.ioc` / project target |
| CubeMX / HAL | CubeMX 6.12.0 / STM32CubeF1 v1.8.7 | Confirmed by project metadata and configured package |
| Clock | HSE 8 MHz × 9; SYSCLK/HCLK 72 MHz; APB1 36 MHz; APB2 72 MHz; APB1 timer clock 72 MHz | Confirmed by `.ioc`, generated clock setup, and `HSE_VALUE` |
| Debug | SWD on PA13/PA14; J-Link connection, download, and debug reported working by the user | Project configuration and user report; no bring-up repeated |
| Servo | TIM2_CH1 on PA0, PSC 71, ARR 19999 | Generated and verified; initial CH1 Pulse 1500 |
| HC-04 UART | USART1 TX PA9 / RX PA10, asynchronous 8N1 | Generated and verified; USART1 global NVIC enabled |
| HC-04 receive | USART1_RX → DMA1 Channel 5, circular, byte/byte, high priority | Generated and verified; DMA1 Channel 5 global NVIC enabled |
| HC-04 baud | 115200 in generated `usart.c` and `Config/project_config.h` | Test default; `TO_BE_CONFIRMED` against the actual module and peer |
| Servo pulse window | 1400–1600 µs, center 1500 µs | `INITIAL_TEST_VALUE`; `CALIBRATION REQUIRED` |
| TB6600 resources | TIM3_CH1/PA6, PB12 DIR, PB13 ENA, DMA1 Channel 6 | Reserved for future work; not configured or implemented |

The `.ioc` now contains TIM2, USART1, DMA1 Channel 5, and both required global interrupts. The generated clock, pin, timer prescaler/period/pulse, UART format, DMA request/channel/mode/widths/increments/priority, and IRQ handlers have been checked. No additional CubeMX peripheral is required for this milestone.

## Clock tree and debug reservation

Keep the existing clock tree unchanged:

```text
HSE 8 MHz → PLL × 9 → SYSCLK 72 MHz
AHB 72 MHz; APB1 36 MHz; APB2 72 MHz
APB1 timer clock = 72 MHz (APB1 prescaler is 2)
```

Keep SWD enabled on PA13/PA14. Do not select full JTAG, remap the debug pins, or assign GPIO functions to PA13/PA14. No Alternate Function remap is planned for the selected project peripherals.

## CubeMX configuration to preserve

Preserve the following settings on future CubeMX regeneration. Keep the existing CMake toolchain selection and the existing “Keep User Code when re-generating” setting.

### 1. TIM2 servo PWM

1. In **Pinout & Configuration → Timers → TIM2**, select **Internal Clock**.
2. Enable **PWM Generation CH1**. Confirm the pin maps to **PA0 / TIM2_CH1**; do not enable a remap.
3. In TIM2 parameter settings, set **Prescaler = 71**, **Counter Period = 19999**, **Counter Mode = Up**, **Clock Division = DIV1**.
4. Keep **PWM mode 1**, **polarity High**, **Fast Mode Disable**, and **CH1 Pulse = 1500**. This is an initial center test value, not a calibrated mechanical center.
5. Do not add a TIM2_CH1 DMA request.

With the confirmed 72 MHz timer clock this yields a 1 MHz timer counter (1 µs per count), 20 ms period, and 50 Hz PWM.

### 2. USART1

1. Enable **USART1 → Asynchronous**. Verify TX is **PA9** and RX is **PA10**, with no remap.
2. Keep **Baud Rate = 115200** to match the current test default, **Word Length = 8 Bits**, **Parity = None**, **Stop Bits = 1**, **Hardware Flow Control = None**, **Mode = TX and RX**.
3. In **NVIC Settings**, keep **USART1 global interrupt** (UART IDLE/error events) and **DMA1 Channel 5 global interrupt** (DMA half-transfer/transfer-complete events) enabled. Do not enable per-byte RX interrupt reception; RX data uses DMA.
4. `Config/project_config.h` marks 115200 as `TO_BE_CONFIRMED`; after confirming the actual module and peer, update config and generated UART setting together if they differ.

### 3. USART1_RX DMA

In **USART1 → DMA Settings**, add the **USART1_RX** request and verify it is **DMA1 Channel 5** with:

| Setting | Value |
|---|---|
| Direction | Peripheral to Memory |
| Mode | Circular |
| Peripheral increment | Disabled |
| Memory increment | Enabled |
| Peripheral data width | Byte |
| Memory data width | Byte |
| Priority | High |

Do not map TIM2_CH1 to DMA1 Channel 5. USART1_TX DMA1 Channel 4 is optional and should remain disabled for this first round.

### 4. Regeneration review

On regeneration, confirm that `tim.c` uses Pulse 1500, `usart.c` matches `HC04_BAUDRATE`, DMA1 Channel 5 is Circular, and both USART1 and DMA1 Channel 5 interrupts are enabled/handled. Current generated handles are `htim2`, `huart1`, and `hdma_usart1_rx`. Do not hand-edit generated peripheral files to implement drivers.

## Resource allocation

| Function | Peripheral / signal | Pin | DMA | Allocation |
|---|---|---|---|---|
| Servo PWM | TIM2_CH1 | PA0 | None | Current milestone |
| HC-04 transmit | USART1_TX | PA9 | DMA1_CH4 optional, currently unused | Current milestone |
| HC-04 receive | USART1_RX | PA10 | DMA1_CH5 | Current milestone |
| TB6600 pulse | TIM3_CH1 | PA6 | DMA1_CH6 reserved | Future reservation |
| TB6600 direction | GPIO | PB12 | None | Future reservation |
| TB6600 enable | GPIO | PB13 | None | Future reservation |
| J-Link SWDIO | SWDIO | PA13 | — | Reserved |
| J-Link SWCLK | SWCLK | PA14 | — | Reserved |

On STM32F103, USART1_RX and TIM2_CH1 share the DMA1 Channel 5 request mapping. This is why the Servo uses hardware PWM with CPU compare-register updates and no DMA. Verify the mapping in the official [RM0008 Reference Manual](https://www.st.com/resource/en/reference_manual/rm0008-stm32f103xx-advanced-armbased-32bit-mcus-stmicroelectronics.pdf).

## Board connections and electrical facts to confirm

- Connect USART1 PA9/PA10 to the HC-04 module's UART RX/TX respectively, and establish a common ground. Confirm the particular module's supply and UART logic-level requirements before wiring; product names sold as “HC-04” are not enough to establish those electrical details.
- Power a servo from a supply appropriate for its actual model and stall current. Join signal ground with the STM32 ground. Do not assume the MCU board's 3.3 V rail can power the servo.
- The actual servo model, pulse range, center, direction, travel limits, and mechanical linkage are not yet recorded. Do not command mechanical endpoints until calibrated.
- A retail TB6600 module may add optocouplers and input circuitry; bare-chip values do not by themselves prove the module's logic-level compatibility. Confirm the exact module before its future integration.

## Servo calibration and configuration

`SERVO_MIN_PULSE_US=1400`, `SERVO_CENTER_PULSE_US=1500`, and `SERVO_MAX_PULSE_US=1600` are narrow initial bench-test values only. They do not establish the servo's true min/max pulse or actual 0°/270° positions. Begin near center and expand in small increments only after checking movement and mechanical clearance. Record measured safe min, center, max, direction, and model; then update `Config/project_config.h` and this document together.

## References

- [STM32F103x8/xB datasheet](https://www.st.com/resource/en/datasheet/stm32f103c8.pdf)
- [ST RM0008 Reference Manual](https://www.st.com/resource/en/reference_manual/rm0008-stm32f103xx-advanced-armbased-32bit-mcus-stmicroelectronics.pdf)
- [STM32F101x8/B, STM32F102x8/B, STM32F103x8/B errata sheet ES096](https://www.st.com/resource/en/errata_sheet/es096-stm32f101x8b-stm32f102x8b-and-stm32f103x8b-mediumdensity-device-limitations-stmicroelectronics.pdf)
- [Toshiba TB6600HG datasheet](https://toshiba.semicon-storage.com/info/docget.jsp?did=12780&prodName=TB6600HG)

The ES096 USART IDLE limitation is one reason the receiver must also consume DMA half-transfer and transfer-complete progress events, with newline as the application framing delimiter. The exact HC-04 and Servo module parameters remain subject to their own hardware documentation and bench confirmation.

# Hardware and CubeMX configuration

## Evidence status

Generated configuration is checked against the `.ioc` and generated sources. Module electrical behavior is not inferred from the name “TB6600” or from the bare-chip datasheet.

| Item | Configuration | Evidence/status |
|---|---|---|
| MCU | STM32F103C8T6, LQFP48 | `.ioc` project target |
| CubeMX / HAL | CubeMX 6.12.0 / STM32CubeF1 v1.8.7 | Project metadata/package used for build |
| Clock | HSE 8 MHz × 9; HCLK 72 MHz; APB1 36 MHz; APB1 timer clock 72 MHz | `.ioc` and generated clock setup |
| Debug | SWD PA13/PA14 | Preserved; user previously reported J-Link working |
| Servo | TIM2_CH1 / PA0, PSC 71, ARR 19999, initial CCR 1500 | Generated and verified; 1 MHz tick, 50 Hz frame |
| HC-04 | USART1 TX PA9 / RX PA10, 8N1, 115200 | Generated/config match; actual module baud remains `TO_BE_CONFIRMED` |
| HC-04 RX DMA | DMA1 Channel 5, circular, byte transfers, high priority | Generated and verified; global IRQ enabled |
| TB6600 PUL | TIM3_CH1 / PA6, PWM1, initial PSC 71, ARR 49999, CCR 10, high polarity | Generated and verified; BSP checks the timer settings and applies configured polarity |
| TB6600 DIR / ENA | PB12 / PB13 push-pull outputs, initial low | Generated and verified; polarity remains a software assumption pending module confirmation |
| TIM3 interrupt | TIM3 global IRQ enabled and dispatches to HAL | Generated and verified; used for CH1 compare events |
| TB6600 module marking | `TB6600`, `DC 9–42VDC` on the user-provided photo | Label transcription only; exact variant and connected supply are unknown |
| Servo pulse window | 1400–1600 µs, center 1500 µs | Narrow initial test window; calibration required |
| TB6600 pulse width | 10 µs active width | Initial software value; actual module requirement unverified |
| Step frequency | 20–10,000 steps/s | Initial software limits; not hardware verified |
| Direction setup | 1 ms nonblocking wait | Initial software value; module timing unverified |
| Stepper motor | 1.8° step angle, 1.5 A reported current; four leads reported connected to `A+`, `A-`, `B+`, `B-` | User-provided; current rating basis and coil pairing not independently verified |
| TB6600 DIP settings | Current and microstep switch positions | Not provided; do not assume from the printed selection table |
| TB6600 signal wiring | Connections to `PUL±`, `DIR±`, `ENA±`, `VCC` and `GND` | Not provided; input topology and 3.3 V compatibility remain unverified |

## Resource allocation

| Function | Peripheral/signal | Pin | DMA | Allocation |
|---|---|---|---|---|
| Servo | TIM2_CH1 | PA0 | None | Active |
| HC-04 TX | USART1_TX | PA9 | DMA1_CH4 optional, unused | Active UART |
| HC-04 RX | USART1_RX | PA10 | DMA1_CH5 circular | Active |
| TB6600 PUL | TIM3_CH1 | PA6 | DMA1_CH6 mapping available, unused | Active software PWM |
| Future profile ARR updates | TIM3_UP | — | **DMA1_CH3 reserved** | Future design only |
| TB6600 DIR | GPIO | PB12 | None | Active |
| TB6600 ENA | GPIO | PB13 | None | Active |
| SWDIO / SWCLK | SWD | PA13 / PA14 | — | Reserved for debug |

USART1_RX and TIM2_CH1 share DMA1 Channel 5 mapping on STM32F103. Do not assign Servo PWM to DMA. Keep DMA1_CH5 dedicated to USART1_RX. Do not enable any Stepper DMA in this milestone.

## Clock and pin constraints

Preserve the current clock tree:

```text
HSE 8 MHz → PLL × 9 → SYSCLK/HCLK 72 MHz
APB1 36 MHz (prescaler 2) → TIM3/TIM2 timer clock 72 MHz
APB2 72 MHz
```

Keep SWD on PA13/PA14; do not enable full JTAG, remap debug pins, or assign GPIO functions to them. TIM3 CH1 must remain on PA6 with no remap.

## CubeMX configuration to preserve

The current `.ioc` and generated output have been checked. Regeneration is user-operated; check the following afterward.

### TIM2 Servo

- Internal clock; PWM Generation CH1 on PA0, no remap.
- PSC 71; counter period 19999; up-counting; DIV1.
- PWM1, polarity High, Fast Mode disabled, pulse 1500.
- No TIM2_CH1 DMA request.

### USART1 and DMA

- USART1 asynchronous, PA9 TX / PA10 RX, no remap, TX+RX, 115200 baud test default, 8 data bits, no parity, 1 stop bit, no flow control.
- USART1 global IRQ and DMA1 Channel 5 global IRQ enabled.
- USART1_RX → DMA1 Channel 5: Peripheral-to-Memory, circular, byte/byte, peripheral increment disabled, memory increment enabled, high priority.
- USART1_TX DMA1 Channel 4 remains disabled; optional future allocation only.

### TIM3 and TB6600 GPIO

- TIM3 internal clock; PWM Generation CH1, mapped to PA6 without remap.
- PSC 71, ARR 49999, up-counting, DIV1; PWM1, polarity High, Fast Mode disabled, initial pulse 10.
- Enable TIM3 global interrupt. Do not enable TIM3 update or channel DMA requests.
- PB12 is GPIO output labeled `TB6600_DIR`; PB13 is GPIO output labeled `TB6600_ENA`; both initial output levels low, push-pull, no pull.
- TIM3 CH1 DMA1 Channel 6 mapping remains unused. Future TIM3_UP profile updates reserve DMA1 Channel 3.
- Preserve CubeMX “Keep User Code when re-generating” and the existing CMake project/toolchain selection.

### Regeneration review

Check `.ioc`, `Core/Src/tim.c`, `Core/Src/gpio.c`, `Core/Inc/main.h`, `Core/Src/stm32f1xx_it.c`, `Core/Src/stm32f1xx_hal_msp.c`, `dma.c`, `usart.c`, and the root CMake source list. Confirm `MX_TIM3_Init()` runs before `App_Init()`, `TIM3_IRQHandler()` calls `HAL_TIM_IRQHandler(&htim3)`, PA6 is configured as AF push-pull, and PB12/PB13 start low.

CubeMX may write local absolute package paths into `cmake/stm32cubemx/CMakeLists.txt`; root `CMakeLists.txt` intentionally does not include that generated file. CubeF1 lookup uses the user-maintained `cmake/stm32cube_f1.cmake`, so the generated file cannot break root builds. Review Core source changes and update the root source list when CubeMX adds/removes generated files.

## Electrical and motion assumptions

The firmware targets a commercial TB6600 driver module. It is not safe to infer module-level input voltage, optocoupler current, polarity, pulse width, or maximum frequency from the bare TB6600HG IC datasheet. Confirm the exact module and its manual before wiring or enabling the motor.

Current initial assumptions in `Config/project_config.h` are active-high ENA, forward DIR high, and active-high PUL. The project labels them `INITIAL_ASSUMPTION` / `TO_BE_CONFIRMED`. Confirm whether the actual module expects common-anode/common-cathode wiring, whether 3.3 V GPIO is sufficient for its optocouplers, ENA behavior, direction mapping, minimum pulse width, and safe maximum step rate. Do not rely on these software values as hardware specifications.

Also confirm the exact Servo model and safe pulse endpoints, and confirm the HC-04 variant, logic-level requirements, pairing and current UART baud before attributing a failed test to firmware.

## References

- [STM32F103x8/xB datasheet](https://www.st.com/resource/en/datasheet/stm32f103c8.pdf)
- [ST RM0008 Reference Manual](https://www.st.com/resource/en/reference_manual/rm0008-stm32f103xx-advanced-armbased-32bit-mcus-stmicroelectronics.pdf)
- [STM32F101x8/B, STM32F102x8/B, STM32F103x8/B errata sheet ES096](https://www.st.com/resource/en/errata_sheet/es096-stm32f101x8b-stm32f102x8b-and-stm32f103x8b-mediumdensity-device-limitations-stmicroelectronics.pdf)
- [Toshiba TB6600HG datasheet](https://toshiba.semicon-storage.com/info/docget.jsp?did=12780&prodName=TB6600HG)

The bare-chip datasheet is listed only as an IC reference; module wiring decisions require the actual module documentation. Physical Servo, HC-04 and TB6600 verification remains `PENDING`.

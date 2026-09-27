# Hardware and CubeMX configuration

## Evidence status

Generated configuration is checked against the `.ioc` and generated sources. Module electrical behavior is not inferred from the name “TB6600” or from the bare-chip datasheet.

| Item | Configuration | Evidence/status |
|---|---|---|
| MCU | STM32F103C8T6, LQFP48 | `.ioc` project target |
| CubeMX / HAL | CubeMX 6.12.0 / STM32CubeF1 v1.8.7 | Project metadata/package used for build |
| Clock | HSE 8 MHz × 9; HCLK 72 MHz; APB1 36 MHz; APB1 timer clock 72 MHz | `.ioc` and generated clock setup |
| Debug | SWD PA13/PA14 | Preserved; user previously reported J-Link working |
| Servo | TIM2_CH1 / PA0, PSC 71, ARR 19999, generated initial CCR 1500; application Pitch zero targets Servo 148° / CCR 1596 | Generated 1 MHz tick and 50 Hz frame; after flashing, live `SERVO?` returned 148 at boot, Ozone CCR1 was reported as 1596 and the user confirmed horizontal |
| HC-04 | USART1 TX PA9 / RX PA10, 8N1, 115200 | Generated/config match; live COM5 `PING`/`PONG` verified 115200 communication on the current bench |
| HC-04 RX DMA | DMA1 Channel 5, circular, byte transfers, high priority | Generated and verified; global IRQ enabled |
| TB6600 PUL | TIM3_CH1 / PA6, PWM1, initial PSC 71, ARR 49999, CCR 10, high polarity | Generated/configuration verified; no-power PA6 scope check showed 20.00 Hz / 50.00 ms. The 50 kSa/s capture cannot resolve 10 µs pulse width or actual high voltage |
| TB6600 DIR / ENA | PB12 / PB13 push-pull outputs, CubeMX initial PB12 LOW and PB13 HIGH | Regenerated configuration verified; powered bench test found PB13 LOW gives holding torque and HIGH releases it; positive pulses rotated Yaw clockwise, negative pulses returned it |
| TIM3 interrupt | TIM3 global IRQ enabled and dispatches to HAL | Generated and verified; used for CH1 compare events |
| TB6600 module marking | PUFEIDE `TB6600`, `DC 9–42VDC` on the user-provided photo | Label transcription; exact board revision unknown; user reported 12.3 V on the connected 12 V supply |
| Servo pulse window | 500–2500 µs, center 1500 µs at Servo 135°; observed platform level at Servo 148° / approximately 1596 µs | User-supplied 270° reference and user-observed platform level; full pulse endpoints remain unmeasured |
| TB6600 pulse width | 10 µs active width | Initial software value; actual module requirement unverified |
| TB6600 timer frequency | 20–10,000 PUL pulses/s | Low-level software/timer range; not module verified |
| Yaw mechanism frequency | 20–500 PUL pulses/s | Initial conservative command limit; not a motor/driver rating |
| Direction setup | 1 ms nonblocking wait | Initial software value; module timing unverified |
| Stepper motor | 1.8° step angle, 1.5 A reported current; four leads reported connected to `A+`, `A-`, `B+`, `B-` | User-provided; current rating basis and coil pairing not independently verified |
| TB6600 DIP settings / Yaw scale | Current assumption: 8 microstep / `YAW_AXIS_PULSES_PER_REV=1600` PUL per output-axis revolution with 1:1 coupling; 1.5 A label row | SW1 OFF, SW2 ON, SW3 OFF; SW4 ON, SW5 ON, SW6 OFF; actual switch positions, ratio and platform angle pending |
| TB6600 wiring topology | Selected 3.3 V direct GPIO, common-cathode wiring | See the [wiring source of truth](wiring.md); ENA, PUL recognition and bidirectional motion observed at 12 V, while GPIO current and loaded waveform remain unmeasured |

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

The Round 3 CubeMX regeneration now records `MX_TIM3_Init` in `ProjectManager.functionlistsort`. Check `.ioc`, `Core/Src/tim.c`, `Core/Src/gpio.c`, `Core/Inc/main.h`, `Core/Src/stm32f1xx_it.c`, `Core/Src/stm32f1xx_hal_msp.c`, `dma.c`, `usart.c`, and the root CMake source list. Confirm `MX_USART1_UART_Init()` then `MX_TIM3_Init()` run before `App_Init()`, `TIM3_IRQHandler()` calls `HAL_TIM_IRQHandler(&htim3)`, PA6 is configured as AF push-pull, and PB12/PB13 start low.

CubeMX may write local absolute package paths into `cmake/stm32cubemx/CMakeLists.txt`; root `CMakeLists.txt` intentionally does not include that generated file. CubeF1 lookup uses the user-maintained `cmake/stm32cube_f1.cmake`, so the generated file cannot break root builds. Review Core source changes and update the root source list when CubeMX adds/removes generated files.

## Selected wiring and pending hardware checks

The project uses a 3.3 V direct-GPIO common-cathode topology and DIP settings based on the user's plan and the pictured module label. The canonical signal, power, motor and switch tables are in [Docs/wiring.md](wiring.md). `Config/project_config.h` uses active-high PUL, bench-observed active-low ENA and HIGH=logical FORWARD; the mechanical direction mapping is still unverified.

Before applying TB6600 driver power, measure the PUL/DIR/ENA input currents and confirm valid 3.3 V logic levels. The project acceptance gate is at most 8 mA per signal. PUL pulse recognition, ENA behavior, DIR orientation, 10 µs pulse-width margin, safe pulse rate, electrical noise and motor rotation remain PENDING. Do not infer module input behavior from the bare TB6600HG IC datasheet.

For STM32 GPIO output conditions, refer to the ST datasheet linked below. PC13/PC14/PC15 are not substitutes for PA6/PB12/PB13 in this selected direct-drive topology.

Also confirm the exact Servo model and safe pulse endpoints, and confirm the HC-04 variant, logic-level requirements, pairing and current UART baud before attributing a failed test to firmware.

## References

- [STM32F103x8/xB datasheet](https://www.st.com/resource/en/datasheet/stm32f103c8.pdf)
- [ST RM0008 Reference Manual](https://www.st.com/resource/en/reference_manual/rm0008-stm32f103xx-advanced-armbased-32bit-mcus-stmicroelectronics.pdf)
- [STM32F101x8/B, STM32F102x8/B, STM32F103x8/B errata sheet ES096](https://www.st.com/resource/en/errata_sheet/es096-stm32f101x8b-stm32f102x8b-and-stm32f103x8b-mediumdensity-device-limitations-stmicroelectronics.pdf)
- [Toshiba TB6600HG datasheet](https://toshiba.semicon-storage.com/info/docget.jsp?did=12780&prodName=TB6600HG)

The bare-chip datasheet is listed only as an IC reference, not as a module input specification. Physical Servo, HC-04 and TB6600 verification remains `PENDING`.

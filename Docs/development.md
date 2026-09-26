# Development and verification

## Environment

- MCU: STM32F103C8T6, LQFP48.
- CubeMX project: version 6.12.0; firmware package STM32CubeF1 v1.8.7.
- Language and build: C11, CMake 3.22+, Ninja, GNU Arm Embedded Toolchain.
- Debugger: J-Link over SWD; the user has confirmed connection, download, and debug. Do not repeat debugger bring-up unless a new concrete failure appears.

## Current implementation and build status

- Implemented: Servo PWM driver, HC-04 circular DMA/Receive-to-Idle transport, bounded command parser, App scheduling, and centralized configuration.
- Debug and Release CMake configure/build passed against local STM32CubeF1 v1.8.7 with GNU Arm Embedded GCC 13.2.1. The compiler emitted no warnings. GNU ld reported an RWX LOAD segment warning in both builds; it is recorded separately from compiler warnings.
- Build memory report: Debug 2,168 bytes RAM / 17,568 bytes Flash; Release 2,184 bytes RAM / 11,180 bytes Flash.
- Physical Servo PWM measurement, Servo pulse calibration, HC-04 electrical/baud confirmation, pairing, and command exchange remain pending.

The build locates the firmware package through `STM32CUBE_F1_FW_ROOT` (CMake cache variable or environment variable). On Windows, the configured default is `%USERPROFILE%/STM32Cube/Repository/STM32Cube_FW_F1_V1.8.7`.

## CubeMX regeneration

1. Open the checked-in `rubbish-sorting.ioc` and preserve HSE × 9 / 72 MHz clocks, APB1 /2, APB2 /1, and SWD on PA13/PA14.
2. Configure TIM2_CH1/PA0 and USART1 PA9/PA10 as specified in [hardware.md](hardware.md).
3. Assign USART1_RX to DMA1 Channel 5 with circular byte transfers; enable both USART1 and DMA1 Channel 5 global NVIC interrupts.
4. Confirm Project Manager still targets CMake and “Keep User Code when re-generating” remains enabled.
5. Generate code. Review `.ioc`, generated `tim.*`, `usart.*`, `dma.*`, `stm32f1xx_it.*`, and `stm32f1xx_hal_msp.*` changes before integrating drivers. Confirm DMA mode is Circular and both USART1 and DMA1 Channel 5 IRQ handlers exist.

CubeMX owns `.ioc`, `Core/*`, and the peripheral source list in `cmake/stm32cubemx/*`. Keep reusable application, BSP, protocol, and configuration code outside generated files. CubeMX may emit absolute CubeF1 installation paths into its CMake file; this project replaces those with the portable `STM32CUBE_F1_FW_ROOT` lookup, so inspect that file after each regeneration and restore the portable lookup if needed. If a generated-file customization is unavoidable, keep it in a CubeMX user-code region and document its regeneration check.

## CMake build

From the repository root (PowerShell):

```powershell
cmake --preset Debug -DSTM32CUBE_F1_FW_ROOT="C:/Users/<user>/STM32Cube/Repository/STM32Cube_FW_F1_V1.8.7"
cmake --build --preset Debug
```

For other presets, replace `Debug` with `RelWithDebInfo`, `Release`, or `MinSizeRel`. Build artifacts are under `build/<preset>/`. Do not report a build as verified until both configure and build complete successfully against the generated project and the stated CubeF1 package.

## First-round hardware verification order

1. **Configuration inspection:** confirm MCU/package, clock tree, SWD, TIM2/PA0 parameters, USART1/PA9/PA10, and DMA1 Channel 5 mapping.
2. **Build inspection:** configure and build all selected CMake presets required for the change; record warnings separately.
3. **PWM scope check:** with servo mechanics unloaded where possible, measure approximately 20 ms period and 1 µs timer resolution; begin at 1500 µs, then test only 1400 and 1600 µs after confirming the actual servo accepts the pulse range. Do not start at 0° or 270°.
4. **UART command check:** use a confirmed baud and compatible electrical interface. Verify `PING`, then `SERVO?`, then `SERVO_US` near center. Verify malformed and out-of-range inputs return an error without moving outside configured bounds.
5. **Recovery check:** test CRLF/LF, a line longer than the 64-byte command buffer, consecutive commands, and recovery after the invalid line. Check DMA/UART counters if exposed.
6. **Mechanical calibration:** increase pulse range in small steps while observing direction, clearance, and stability. Record the actual Servo model and safe limits before expanding the configured window.

## HC-04 details to confirm

Before interpreting failed communication as a firmware defect, confirm the exact module variant, supply/logic levels, current baud, pairing state, and the peer's UART settings from that module's documentation or direct configuration. `115200` in project configuration is only a provisional test value matching generated code. No default password, AT mode, or electrical level is assumed here.

## Evidence labels

Report these independently:

- **Configuration checked:** verified from `.ioc` and generated peripheral setup.
- **Build verified:** CMake configure/build completed; include build type and warnings.
- **Static review checked:** interfaces, DMA ownership, bounds, ISR workload, and generated-code boundary reviewed.
- **Hardware verified:** measured/tested on the physical board and actual attached modules.

A successful build does not prove Servo movement, Bluetooth pairing, electrical compatibility, or mechanical calibration. Record those only after the corresponding physical checks.

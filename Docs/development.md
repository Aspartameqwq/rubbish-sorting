# Development and verification

## Toolchain

- MCU: STM32F103C8T6 (medium-density, 64 KB Flash / 20 KB RAM target).
- CubeMX 6.12.0; STM32CubeF1 HAL v1.8.7.
- Firmware: C11, CMake 3.22+, Ninja, GNU Arm Embedded GCC.
- Host tests: native C11 compiler, CMake, Ninja and CTest.
- `STM32CUBE_F1_FW_ROOT` may be supplied as a CMake cache variable or environment variable. Windows default is `%USERPROFILE%/STM32Cube/Repository/STM32Cube_FW_F1_V1.8.7`.

## Build and test commands

```powershell
cmake --preset Debug
cmake --build --preset Debug

cmake --preset Release
cmake --build --preset Release

cmake -S tests/host -B build/host -G Ninja
cmake --build build/host
ctest --test-dir build/host --output-on-failure
```

Host tests use the native compiler and replace only HAL/TIM/GPIO and UART transport boundaries with local stubs. The actual Servo, TB6600 timing/BSP, Stepper, protocol, and profile C files are compiled into the host test executable. The profile C module is HAL-free.

## Round 2 verification record

- Host C test target: passed; 40,558 checks cover Servo logical/raw state and bounds, command framing/parsing and response, exact 1/2/10/100 pulse termination, graceful stop/direction setup, signed steps including `INT32_MIN`, timing conversion, profile symmetry/range and extreme inputs, plus TB6600 startup, GPIO direction/enable levels and active-high PWM configuration.
- Debug: configure/build passed. RAM 2,296 bytes of 20 KB; Flash 24,544 bytes of 64 KB.
- Release: configure/build passed. RAM 2,304 bytes of 20 KB; Flash 16,724 bytes of 64 KB.
- Firmware compile produced no compiler warnings after the Servo lower-bound check was made warning-free. GNU ld continues to report the existing `LOAD segment with RWX permissions` linker warning; linker-script changes are outside this round.
- CubeMX files were reviewed after user generation: `.ioc`, TIM3 setup, PA6 AF push-pull, PB12/PB13 GPIO initialization, `TIM3_IRQHandler`, and `MX_TIM3_Init()` before `App_Init()`. The checked CMake build resolved CubeF1 through the user-managed module instead of the generated CMake file.
- The selected TB6600 wiring and DIP settings are documented in [wiring.md](wiring.md). Electrical and motion checks remain `PENDING`; no board movement, module input-current or waveform measurement, HC-04 pairing, or Servo calibration was performed.

## Static review checklist

Before publishing a branch or updating this record:

1. Check the branch base, working tree and generated-code diff.
2. Confirm no peripheral access leaks above the BSP and no HAL include leaks into the Motion profile.
3. Review integer overflow, signed parsing, exact pulse termination, direction setup and IRQ callback work.
4. Run host tests and clean Debug/Release builds; distinguish compiler warnings from linker warnings.
5. Confirm docs describe open-loop commanded position and hardware assumptions as unverified.
6. Review CubeMX `.ioc`/generated source and root CMake source list as one configuration change.

## CubeMX regeneration

Preserve clock tree, SWD, TIM2 Servo, USART1, DMA1_CH5 RX, TIM3_CH1/PA6, PB12 DIR, PB13 ENA and TIM3 IRQ as described in [hardware.md](hardware.md). CubeMX owns `.ioc` and `Core/*`; inspect those changes after generation.

The root build does not include `cmake/stm32cubemx/CMakeLists.txt`, where CubeMX can emit local package paths. CubeF1 package lookup and HAL source locations live in `cmake/stm32cube_f1.cmake`; the explicit generated-Core list is in root `CMakeLists.txt`. Review/synchronize it whenever CubeMX adds or removes source files. Keep reusable BSP/Motion/Protocol code and documentation outside generated paths.

## Physical verification order

### TB6600 first hardware bring-up

The project-selected common-cathode signal wiring, 24 V power connection, motor terminals and DIP positions are maintained only in [Docs/wiring.md](wiring.md). Selected wiring is not hardware verification. Keep driver power off until all signal inputs pass the measured-current and logic-level gate.

1. With all power off, confirm switch directions/settings, motor coil pairs, terminal wiring and 24 V polarity.
2. Power only the STM32. Check PB12/PB13 LOW, PA6 idle with no PUL edges, and verify no 24 V reaches any MCU pin.
3. Measure active input current and voltage for PUL, DIR and ENA. The project direct-drive gate is at most 8 mA per signal; for pulsed PUL, use a shunt and scope/peak measurement rather than relying on a DMM average. Stop if any line exceeds the gate or 3.3 V is not recognized reliably.
4. After the electrical gate passes, power the TB6600 from the selected 24 V supply. Check ENA LOW/HIGH physical behavior and capture PUL idle level, high/low widths, frequency and clean stop edges.
5. Using a mechanically safe motor at the minimum software rate, verify exactly 1, 2, 10 and 100 PUL pulses, direction setup and logical direction. Record actual motor movement and any missed steps.
6. Record module marking/revision, supply voltage, DIP positions, input currents, waveform values, ENA behavior, direction and movement. Keep every unmeasured result PENDING.

No physical TB6600 verification has been performed by Codex. The values in software are initial limits until the measurement record supports any change.

Also test Servo near center before endpoint exploration and calibrate the actual model and safe pulse range. Confirm HC-04 variant, UART baud, supply/logic levels and pairing before link tests.

Keep **configuration checked**, **build verified**, **static review checked**, and **hardware verified** as separate claims. A build does not prove module compatibility, emitted waveform quality, motor movement, shaft position or Servo calibration.

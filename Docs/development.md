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

- Host C test target: passed; 40,542 checks cover Servo logical/raw state and bounds, command framing/parsing and response, exact 1/2/10/100 pulse termination, graceful stop/direction setup, signed steps including `INT32_MIN`, timing conversion, profile symmetry/range and extreme inputs.
- Debug: configure/build passed. RAM 2,296 bytes of 20 KB; Flash 24,544 bytes of 64 KB.
- Release: configure/build passed. RAM 2,304 bytes of 20 KB; Flash 16,724 bytes of 64 KB.
- Firmware compile produced no compiler warnings after the Servo lower-bound check was made warning-free. GNU ld continues to report the existing `LOAD segment with RWX permissions` linker warning; linker-script changes are outside this round.
- CubeMX files were reviewed after user generation: `.ioc`, TIM3 setup, PA6 AF push-pull, PB12/PB13 GPIO initialization, `TIM3_IRQHandler`, and `MX_TIM3_Init()` before `App_Init()`. The checked CMake build resolved CubeF1 through the user-managed module instead of the generated CMake file.
- Hardware checks remain `PENDING`; no board movement, module wiring, waveform measurement, HC-04 pairing, or Servo calibration was performed.

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

No physical verification has been done in this round. Before enabling a real driver or moving a motor:

1. Confirm the exact commercial TB6600 module input topology, optocoupler current, 3.3 V compatibility, common ground, ENA/DIR/PUL polarity and motor current settings.
2. Confirm pulse width and safe maximum frequency from the module documentation and measurement; current values are initial software assumptions only.
3. With the motor disconnected, scope PUL inactive level, pulse width and rate. Verify exactly 1, 2, 10 and 100 finite pulses and stop edges.
4. Verify DIR setup time and direction mapping before connecting a mechanically safe motor.
5. Test Servo near center before any endpoint exploration; calibrate the actual model and safe pulse range.
6. Confirm HC-04 variant, UART baud, supply/logic levels and pairing before link tests.

Keep **configuration checked**, **build verified**, **static review checked**, and **hardware verified** as separate claims. A build does not prove module compatibility, emitted waveform quality, motor movement, shaft position or Servo calibration.

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

## Round 3 verification record

This is a historical snapshot of the preceding branch. Round 4 replaces `g_debug_state` and `g_debug_command` with `g_control_debug`; use the Round 4 record below for the current interface.

- Host CTest: three targets passed. Debug-control target: 40,671 checks; Release-control-disabled target: 40,665 checks; enabled-placeholder-limits target: 40,636 checks; all reported 0 failures.
- The host coverage includes Pitch's 130°→1500 µs logical anchor and PWM-quantized command estimate; 0°/270° endpoints; raw pulse invalidation; Yaw 1600 PUL/rev and signed nearest-pulse conversion; 225 mdeg/PUL; manual zero offset; requested/quantized target distinction; enabled/disabled limit behavior; sensor-invalid telemetry; heartbeat timing; mailbox sequence/acknowledgment; and Release no-motion behavior.
- Debug: configure/build passed. RAM 2,520 bytes of 20 KB; Flash 30,788 bytes of 64 KB.
- Release: configure/build passed. RAM 2,528 bytes of 20 KB; Flash 19,336 bytes of 64 KB.
- No compiler warnings were observed. GNU ld continues to report the existing `LOAD segment with RWX permissions` warning.
- `arm-none-eabi-nm` found external `g_debug_state` and `g_debug_command` symbols in both Debug and Release ELFs. This confirms link-visible symbols, not an Ozone/J-Link hardware session.
- The user regenerated CubeMX output. `.ioc` now lists `MX_TIM3_Init` in `ProjectManager.functionlistsort`; `main.c` calls it after `MX_USART1_UART_Init()` and before `App_Init()`. TIM3 PSC 71, ARR 49999, CH1 pulse 10, IRQ and PA6 mapping were reviewed. CubeMX's rewritten generated CMake file contained absolute local package paths and was excluded; the root build uses the reviewed package resolver.
- Host simulation and target builds do not verify Servo calibration, TB6600 signal-return continuity, GPIO input current, 3.3 V logic recognition, actual pulse waveform, motor direction/movement or physical angle feedback. Those checks remain `PENDING`.

## Round 4 verification record (initial safety/smoothing change)

This is the verification snapshot for the preceding PR #5 head, before the
fractional-center follow-up below.

- Host CTest: all three targets passed. Debug-control/raw-bench enabled: 40,760 checks; Release-control/raw-bench disabled: 40,725 checks; Yaw-limit variant: 40,731 checks; all reported 0 failures.
- Coverage includes Pitch 0° horizontal coordinate and the observed 130° Servo anchor, fixed ±30° software limits, raw angle/pulse conversion checks, normal signed `PITCH` protocol parsing, Release raw-command rejection, 20 ms trajectory updates, 1000 ms midpoint interpolation, retarget continuity, 200–5000 ms response-time validation/latching, runtime Ozone tuning, command acknowledgement/clearing, and even/odd snapshot sequencing.
- ARM Debug build passed. RAM 2,600 bytes of 20 KB; Flash 33,260 bytes of 64 KB.
- ARM Release build passed. RAM 2,608 bytes of 20 KB; Flash 18,780 bytes of 64 KB.
- No compiler warnings were observed. GNU ld reports the existing `LOAD segment with RWX permissions` warning.
- `arm-none-eabi-nm` found the external `g_control_debug` symbol in both ELFs. This confirms linked global data, not a live Ozone/J-Link session.
- `git diff --check` passed. The source audit found application Protocol/Diagnostics call through PitchAxis/YawAxis, with only the documented Debug-gated raw paths reaching low-level bench APIs.
- Software builds and host tests do not verify physical Servo travel/direction, actual safe pulse range, board-level input/output levels, TB6600 waveform, motor movement or mechanical clearance. These remain `PENDING`.

## Round 4 final review fix verification record

- Host CTest: all four targets passed. Debug: 40,762 checks; Release: 40,727; Yaw-limit variant: 40,733; fractional-center variant: 40,772; all reported 0 failures.
- The fractional-center target compiles with `PITCH_LEVEL_SERVO_MDEG=130500L` and verifies Pitch 0 maps to Servo 130500 mdeg, the center pulse remains 1500 µs, and pulse-to-angle conversion returns 130500 mdeg without integer-degree truncation.
- ARM Debug build passed. RAM 2,600 bytes of 20 KB; Flash 33,260 bytes of 64 KB.
- ARM Release build passed. RAM 2,608 bytes of 20 KB; Flash 18,780 bytes of 64 KB.
- No new compiler warnings were observed. GNU ld reports the existing `LOAD segment with RWX permissions` warning.
- `arm-none-eabi-nm` confirms `g_control_debug` remains in both ELFs; legacy `g_debug_state` and `g_debug_command` symbols remain absent.
- Servo center comparisons and angle/pulse conversion use `SERVO_CENTER_ANGLE_MDEG`; the integer-degree compatibility macro is display-only. Application, Protocol, and Diagnostics have no direct low-level Servo actuation calls.
- `Docs/wiring.md` now contains the system connection map and an unfilled physical measurement record. Ozone variable meanings and staged operating examples are documented in `Docs/debugging.md`.
- All hardware measurements and live Ozone/J-Link checks remain `PENDING`; CubeMX peripherals were not changed in this follow-up.

## Yaw cable-wrap safety verification record

- Host CTest: all four targets passed: Debug bench enabled, Release bench gated, mandatory Yaw limits with the legacy `YAW_SOFT_LIMIT_VALID=0` definition, and fractional Pitch-center configuration.
- Coverage includes the fixed ±180000 mdeg / ±800 PUL boundaries, rejected out-of-range and quantized targets, relative-pulse guards in both directions, cable margins, invalid boot reference, disabled-only zeroing, STOP reference preservation, DISABLE invalidation, Ozone invalid-angle telemetry, and the +170° to -170° linear reverse path (1512 pulses).
- ARM Debug build passed. RAM 2,640 bytes of 20 KB; Flash 34,496 bytes of 64 KB.
- ARM Release build passed. RAM 2,648 bytes of 20 KB; Flash 19,484 bytes of 64 KB.
- No compiler warnings were observed. GNU ld continues to report the existing `LOAD segment with RWX permissions` warning.
- `arm-none-eabi-nm` found `g_control_debug` in both Debug and Release ELFs. Existing Yaw debug fields remain in place; cable telemetry is appended, and `DEBUG_STATE_VERSION` is 3.
- Static source review confirms all application `Stepper_Enable`, `Stepper_Disable`, `Stepper_Stop` and `Stepper_MoveSteps` calls are inside YawAxis; UART Stepper commands call YawAxis wrappers. No CubeMX files were changed.
- Cable limits use an operator-established open-loop reference and cannot detect missed steps, physical hand movement while disabled, or cable/mechanical condition. Ozone/J-Link and all physical motor, current, voltage and clearance checks remain `PENDING`.

## Yaw cable-wrap final review follow-up

- Host CTest: all four targets passed. Direct target counts: Debug 41,029; Release-control-disabled 40,949; Yaw-limits 41,029; fractional-center 41,039; all reported 0 failures.
- Coverage adds output-axis scale and derived pulse bounds, 20/500 PUL/s acceptance and 19/501 rejection through angle and relative APIs, Protocol acceptance at 500 and rejection at 501, all Debug command enum values 0–9, disable during a running move, idle disable, STOP preservation, fault invalidation, and `cable_margin_valid` telemetry.
- ARM Debug build passed. RAM 2,664 bytes of 20 KB; Flash 34,804 bytes of 64 KB. ARM Release build passed. RAM 2,672 bytes of 20 KB; Flash 19,704 bytes of 64 KB.
- No compiler warnings were observed. GNU ld continues to report the existing `LOAD segment with RWX permissions` warning.
- `arm-none-eabi-nm` found `g_control_debug` in both ELFs and found no `g_debug_state` or `g_debug_command`. `DEBUG_STATE_VERSION` is 4; prior `YawDebugState` fields remain ordered as before and new fields are appended.
- Static review confirms Yaw production conversion uses `YAW_AXIS_PULSES_PER_REV`; ±800 PUL is derived from configured angle bounds. All application Stepper actuation remains behind YawAxis. No CubeMX, generated peripheral mapping, Pitch limit/smoothing, Debug command numbering, or selected DIP configuration changed.
- `git diff --check` passed. Ozone/J-Link observation and physical DIP, current, waveform, transmission ratio, pulse-to-platform angle, motor, and cable-clearance checks remain `PENDING`; `YAW_AXIS_SCALE_VERIFIED=0`.

## Static review checklist

Before publishing a branch or updating this record:

1. Check the branch base, working tree and generated-code diff.
2. Confirm no peripheral access leaks above the BSP and no HAL include leaks into the Motion profile; verify the only control/debug configuration surface is synchronized.
3. Review integer overflow, signed parsing, Pitch-to-Servo coordinate conversion and hard limits, trajectory timing/retargeting, exact pulse termination, direction setup and IRQ callback work.
4. Run host tests and clean Debug/Release builds; distinguish compiler warnings from linker warnings.
5. Confirm docs describe open-loop commanded position and hardware assumptions as unverified.
6. Review CubeMX `.ioc`/generated source and root CMake source list as one configuration change.

## CubeMX regeneration

Preserve clock tree, SWD, TIM2 Servo, USART1, DMA1_CH5 RX, TIM3_CH1/PA6, PB12 DIR, PB13 ENA and TIM3 IRQ as described in [hardware.md](hardware.md). CubeMX owns `.ioc` and `Core/*`; inspect those changes after generation.

The root build does not include `cmake/stm32cubemx/CMakeLists.txt`, where CubeMX can emit local package paths. CubeF1 package lookup and HAL source locations live in `cmake/stm32cube_f1.cmake`; the explicit generated-Core list is in root `CMakeLists.txt`. Review/synchronize it whenever CubeMX adds or removes source files. Keep reusable BSP/Motion/Protocol code and documentation outside generated paths.

## Physical verification order

### TB6600 first hardware bring-up

The project-selected common-cathode signal wiring, 24 V power connection, motor terminals and DIP positions are maintained only in [Docs/wiring.md](wiring.md). Selected wiring is not hardware verification. Keep driver power off until the measured-current and MCU output-voltage checks pass; verify powered-module logic recognition during the staged first bring-up.

1. With all power off, confirm switch directions/settings, motor coil pairs, terminal wiring and 24 V polarity.
2. Power only the STM32. Check PB12/PB13 LOW, PA6 idle with no PUL edges, and verify no 24 V reaches any MCU pin.
3. Measure active input current and MCU-driven voltage for PUL, DIR and ENA. The project direct-drive current gate is at most 8 mA per signal; for pulsed PUL, use a shunt and scope/peak measurement rather than relying on a DMM average. Stop if any line exceeds the gate or the loaded MCU output falls outside its datasheet-guaranteed range. Powered-module logic recognition remains a separate check after driver power is applied.
4. After the electrical gate passes, power the TB6600 from the selected 24 V supply. Check ENA LOW/HIGH physical behavior and capture PUL idle level, high/low widths, frequency and clean stop edges.
5. Using a mechanically safe motor at the minimum software rate, verify exactly 1, 2, 10 and 100 PUL pulses, direction setup and logical direction. Record actual motor movement and any missed steps.
6. Record module marking/revision, supply voltage, DIP positions, input currents, waveform values, ENA behavior, direction and movement. Keep every unmeasured result PENDING.

No physical TB6600 verification has been performed by Codex. The values in software are initial limits until the measurement record supports any change.

Also test Servo near center before endpoint exploration and calibrate the actual model and safe pulse range. Confirm HC-04 variant, UART baud, supply/logic levels and pairing before link tests.

Keep **configuration checked**, **build verified**, **static review checked**, and **hardware verified** as separate claims. A build does not prove module compatibility, emitted waveform quality, motor movement, shaft position or Servo calibration.

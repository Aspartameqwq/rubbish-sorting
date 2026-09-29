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

## 2026-09-27 Pitch Servo reference and horizontal calibration

- The user supplied a 270° Servo reference of 500/1500/2500 µs at 0°/135°/270°, confirmed a 1:1 Servo-to-platform angle ratio, and confirmed mechanical clearance for ±45° Pitch. Firmware now uses that pulse mapping and a mandatory `-45000..+45000 mdeg` Pitch command range.
- Live COM5 (115200 baud) returned `PONG`, accepted `SERVO_US 1510`, and read back `SERVO RAW 1510`. `PITCH 30000` was accepted and read back `SERVO 165`; the user saw a clear change from slight forward tilt to pronounced backward tilt. UART `SERVO 140`, `145`, and `148` were each accepted and read back; the user confirmed the platform was horizontal at Servo 148°.
- `PITCH_LEVEL_SERVO_MDEG=148000` now records that observed horizontal angle. The corresponding calculated PWM target is 1596 µs. `SERVO_CALIBRATION_VALID` remains 0 because the full Servo pulse/angle endpoints have not been measured; the horizontal observation alone does not establish the complete calibration.
- Before the updated pulse mapping, live Ozone showed TIM2 CCR1 = 1477 and the PA0 high width was about 1.48 ms. The full ±45° physical travel has not been measured in this session.
- After the 148° source change, host CTest passed 5/5, including 1596 µs horizontal PWM and 1263/1930 µs ±45° boundary expectations. ARM Debug and Release builds passed. GNU ld still reports the existing `LOAD segment with RWX permissions` warning. These software checks do not replace the post-flash physical zero check.
- After the user flashed and ran the new Debug ELF, live COM5 returned `SERVO 148` at startup. `PITCH 0` returned `OK` and the completed state remained `SERVO 148`. The user confirmed the platform stayed horizontal and reported TIM2 CCR1 = 1596 in Ozone. Their Watch screenshot showed `target_mdeg=0`, `commanded_mdeg=0`, `servo_target_mdeg=148000`, `servo_pulse_us=1596`, `moving=0`, `version=4`, a running heartbeat and an even snapshot sequence. The earlier reported 0 was the Pitch target field, not the Servo target field.

## 2026-09-27 Yaw no-driver-power bench check

- With TB6600 driver power disconnected, the user set a manual cable-neutral Yaw zero in Ozone. The Watch showed `reference_state=1`, `commanded_mdeg=0`, `stepper_state=1` (disabled), and `enabled=0`. COM5 `STEPPER ENABLE` returned `OK` and changed the state to `IDLE`.
- COM5 `STEPPER MOVE 100 20` returned `OK`; while running the firmware reported `POS=17 REM=83 FREQ=20`, then `IDLE POS=100 REM=0`. Additional no-power tests of -200 and +600 pulses completed at the expected software counts. After a new manual zero at software POS=500, a further +600-pulse test completed at POS=1100. The interface was then disabled; the current software pulse count reflects no-power tests, not physical Yaw rotation.
- The user's PA6 scope screenshot showed 20.00 Hz and 50.00 ms at 20 ms/div with a 50 kSa/s sample rate. That rate has approximately 20 µs between samples and cannot resolve the configured 10 µs active pulse. The displayed spike shape and clipped `>` peak readings do not establish actual high-level voltage or pulse width. The user chose to stop further waveform testing. TB6600 input current, powered motor response, direction and physical angle remain unverified; a fresh physical cable-neutral zero is required before powered motion.

## 2026-09-27 Yaw 12 V bring-up and ENA polarity correction

- After Ozone restored cable-neutral zero, the user connected a 12 V driver supply and reported 12.3 V. With the old firmware reporting `DISABLED`, COM5 returned three consecutive `PONG` replies, `STEPPER DISABLED POS=1100`, and `SERVO 148`. `STEPPER ENABLE` returned `OK`, then `IDLE`; COM5 stayed responsive. Powered 20- and 100-pulse commands at 20 PUL/s completed in firmware at POS=1120 and POS=1220, with `PONG` after each, but the user saw no motor rotation.
- While the old firmware reported `ENABLE` (PB13 HIGH), the user felt almost no holding torque. When Codex sent `STEPPER DISABLE` (PB13 LOW), the user felt obvious holding torque and measured A+ to A- as -2.223 V with a DC meter. This identifies the installed module's ENA function as active-low; the phase-voltage reading alone does not measure phase current. Codex then restored PB13 HIGH with the old firmware's `STEPPER ENABLE`, and the user disconnected the 12 V supply.
- `TB6600_ENABLE_ACTIVE_LEVEL` is now 0, so the BSP drives PB13 HIGH when logically disabled and LOW when enabled. The user regenerated CubeMX configuration with PB13 initial HIGH and PB12 initial LOW; `.ioc` and `Core/Src/gpio.c` were checked. The CubeMX-generated CMake file contained machine-local package paths and was restored to the repository stub; newline-only generated changes were also removed. Host CTest passed 5/5 after the polarity change. The user reflashed the Debug ELF; startup returned `STEPPER DISABLED POS=0`, PB13 was about 3 V, and the motor had no holding torque under 12 V. `STEPPER ENABLE` brought PB13 to 0 V and produced holding torque.
- Initial powered 20- and 100-pulse commands with corrected ENA polarity advanced the software position but did not visibly rotate the motor. The user found and tightened a loose PUL terminal with driver power disconnected. After a new manual cable-neutral zero at software POS=120, `STEPPER MOVE 100 20` completed at POS=220 and the platform visibly rotated clockwise by roughly 25°; `STEPPER MOVE -100 20` returned to POS=120 and the original mechanical position. The user saw comparable vibration in both directions.
- The runtime Yaw move uses fixed-frequency open-loop TIM3 pulses, with no PID or encoder feedback. At 50 PUL/s, +100 pulses rotated normally with less vibration than at 20 PUL/s; -100 pulses returned to POS=120. At 100 PUL/s, +100 pulses again rotated normally and more smoothly; -100 pulses returned to POS=120. The driver was then disabled, and the user confirmed the platform was back at the original mechanical position with holding torque gone. The frequency-dependent vibration is consistent with slow discrete stepping or a mechanical resonance, but its exact source remains unmeasured. The roughly 25° visual estimate is close to the nominal 22.5° for 100/1600 revolution; it is not a calibrated scale measurement.
- The earlier HC-04 link loss under driver power did not recur in this 12 V session: COM5 at 115200 returned `PONG` through ENA changes and 20/50/100 PUL/s powered moves. This establishes end-to-end communication for the tested sequence, not long-term interference immunity. Input current and loaded GPIO levels remain unrecorded.

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
- Coverage includes the fixed ±180000 mdeg / ±800 PUL boundaries, rejected out-of-range and quantized targets, relative-pulse guards in both directions, cable margins, invalid standalone YawAxis reference, disabled-only zeroing, STOP reference preservation, DISABLE invalidation, Ozone invalid-angle telemetry, and the +170° to -170° linear reverse path (1512 pulses). The application now separately tests its startup-assumed zero.
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

The project-selected common-cathode signal wiring, current 12 V bench power connection, motor terminals and DIP positions are maintained only in [Docs/wiring.md](wiring.md). Selected wiring alone is not hardware verification. Measure input current and loaded MCU output voltage before treating direct-drive compatibility as verified; verify powered-module logic recognition during staged bring-up.

1. With all power off, confirm switch directions/settings, motor coil pairs, terminal wiring and 12 V supply polarity.
2. Power only the STM32. Check PB12 LOW and PB13 HIGH after initialization, PA6 idle with no PUL edges, and verify no driver-supply voltage reaches any MCU pin.
3. Measure active input current and MCU-driven voltage for PUL, DIR and ENA. The project direct-drive current gate is at most 8 mA per signal; for pulsed PUL, use a shunt and scope/peak measurement rather than relying on a DMM average. Stop if any line exceeds the gate or the loaded MCU output falls outside its datasheet-guaranteed range. Powered-module logic recognition remains a separate check after driver power is applied.
4. After the electrical gate passes, power the TB6600 from the selected 12 V supply. Check that ENA LOW holds the motor and HIGH releases it, then capture PUL idle level, high/low widths, frequency and clean stop edges.
5. Using a mechanically safe motor at the minimum software rate, verify exactly 1, 2, 10 and 100 PUL pulses, direction setup and logical direction. Record actual motor movement and any missed steps.
6. Record module marking/revision, supply voltage, DIP positions, input currents, waveform values, ENA behavior, direction and movement. Keep every unmeasured result PENDING.

The live bench check confirmed the ENA polarity and 20 Hz PA6 period, but not powered motor rotation or PUL high-level voltage/width. The remaining software values are initial limits until measurement supports any change.

Also test Servo near center before endpoint exploration and calibrate the actual model and safe pulse range. Confirm HC-04 variant, UART baud, supply/logic levels and pairing before link tests.

Keep **configuration checked**, **build verified**, **static review checked**, and **hardware verified** as separate claims. A build does not prove module compatibility, emitted waveform quality, motor movement, shaft position or Servo calibration.

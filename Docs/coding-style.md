# Coding style and implementation rules

## C and state

- Use C11, fixed-width integer types, `bool` and explicit include guards.
- Public types/functions use the module name and PascalCase; macros use upper snake case; private state and helpers are `static`.
- Keep public headers limited to public types and APIs. Document units and state semantics where a name alone is not enough.
- Use checked integer arithmetic and bounded static buffers. No `malloc`, `calloc`, `realloc` or `free` in this firmware milestone.
- Put shared timing, polarity, buffer and range parameters in `Config/project_config.h`. Mark unverified hardware assumptions `INITIAL_ASSUMPTION` / `TO_BE_CONFIRMED`.

## HAL and module boundaries

- `servo.c` alone owns TIM2_CH1 start/stop and compare updates; no Servo DMA.
- `hc04.c` alone owns USART1, DMA1_CH5 RX state, and UART HAL calls. TX timeouts must be finite; never use `HAL_MAX_DELAY`.
- `tb6600.c` alone owns TIM3_CH1, PB12 and PB13. PUL timing must use hardware PWM, never GPIO bit-banging or `HAL_Delay()`.
- `stepper.c` uses only the public TB6600 API and wrap-safe tick query. It owns requested steps and commanded position, not electrical pins or peripheral handles.
- `stepper_profile.c` must remain pure HAL-free integer mathematics; do not introduce floating point or allocation.
- `protocol.c` calls public module APIs and transport send/read APIs. It must not access HAL, registers, DMA handles or GPIO.
- `App` checks and records init outcomes, then schedules bounded/nonblocking module work.

## Arithmetic, parsing and buffers

- Check every length, index, cast boundary, multiplication and addition before use.
- Do not silently clamp external values. Validate in Protocol and revalidate at the lower-level API.
- Parse UART lines with explicit lengths and digit-by-digit overflow checks. Reject malformed signs, extra tokens and trailing junk.
- Handle `INT32_MIN` without negating a 32-bit signed value or calling `abs(int32_t)`.
- Keep one fixed Protocol line buffer; do not add another UART ring buffer or copy chain over circular DMA.
- `Protocol_Process()` has a fixed byte budget so receive backlog cannot monopolize the main loop.

## Interrupt and timer rules

- Keep callbacks short: capture an event, update a bounded counter/flag, and perform only a required hardware cutoff. Never parse commands, format strings, transmit UART replies, change direction or delay in an ISR.
- TIM3 compare callback counts a completed PUL active width. On the final requested pulse it stops PWM at that compare edge to prevent an extra period; Stepper state and position are reconciled in the main loop.
- Graceful stop occurs at a complete pulse boundary. Document the possible partial pulse from immediate emergency stop.
- Check every HAL return value and use named status enums. Avoid unchecked HAL setup or runtime reinitialization of CubeMX-owned configuration.

## CubeMX, CMake and docs

- CubeMX owns `.ioc`, `Core/*` and generated `cmake/stm32cubemx/*`; keep reusable code outside those paths.
- Root CMake owns a reviewed generated-source list and uses `cmake/stm32cube_f1.cmake` for package resolution. Do not add machine-local absolute paths to tracked files.
- After CubeMX regeneration, review `.ioc`, generated C sources/headers/IRQs and source-list changes together.
- Add every project `.c` file and include directory explicitly to the firmware CMake target. Host tests must use a native compiler separately from the ARM toolchain.
- Keep compiler warnings at zero. Do not suppress a warning, cast away a meaningful type, or remove `const` without a justified fix.
- Update public docs whenever API, protocol, hardware resource, timing parameter or state semantics change.

## Scope exclusions

This round does not implement Stepper DMA acceleration, current/microstep DIP control, homing, limit switches, encoders, closed-loop control, S-curves, multiple axes, FreeRTOS, K230 integration or garbage-sorting business logic.

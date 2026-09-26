# Coding style and implementation rules

This document applies to new STM32 firmware code. Keep code and documentation synchronized whenever a public API, hardware resource, parameter, parser behavior, or module responsibility changes.

## Naming and files

- Public types and functions use module names and PascalCase, e.g. `ServoStatus`, `Servo_Init()`, `HC04_Send()`.
- Macros use upper snake case, e.g. `SERVO_MAX_ANGLE_DEG`, `HC04_RX_DMA_BUFFER_SIZE`.
- Local/private names use lower snake case with a domain meaning, e.g. `angle_deg`, `pulse_us`, `consumer_pos`.
- Public headers declare only public types, constants, and APIs. Keep private state and helper functions in `.c` files.
- Use traditional include guards consistently. Put the current module header first, then standard headers, HAL/Core headers, and other project headers.
- Use `static` for functions and state that do not need cross-file linkage. Avoid exposed global state and use APIs across module boundaries.

## Types and configuration

- Prefer fixed-width integer types (`uint8_t`, `uint16_t`, `uint32_t`, `int32_t`) and `bool` where appropriate.
- Servo angle and pulse APIs use integer types; do not add floating-point Servo calculations.
- Put hardware and project limits in `Config/project_config.h`; avoid scattered baud, pulse, buffer-size, or timeout literals.
- Use checked integer arithmetic. Use at least 32-bit intermediates for angle-to-pulse mapping.
- Do not silently clamp invalid external commands. Validate in Protocol and revalidate safety limits in the driver.

## Memory and buffers

- Do not use `malloc`, `calloc`, `realloc`, or `free` in this firmware milestone.
- Use bounded static storage. HC-04 DMA buffer is 256 bytes; command line buffer is 64 bytes.
- Check every length, index, and numeric conversion before accessing or writing a buffer.
- Do not add another byte ring buffer or copy chain on top of the circular DMA buffer. Keep only the protocol line buffer beyond the DMA transport storage.
- Reject truncated, overflowing, signed, or trailing-garbage numeric input.

## HAL and hardware boundaries

- Only `servo.c` may start TIM2_CH1 PWM or update its compare value. Servo uses hardware PWM and CPU compare updates; it must not use DMA1 Channel 5.
- Only `hc04.c` owns USART1/HAL UART transport and RX DMA state. Use finite TX timeouts; never `HAL_MAX_DELAY`.
- Protocol code calls public BSP APIs and must not call `HAL_UART_*`, use `__HAL_TIM_*`, or access peripheral handles/registers directly.
- App initializes and schedules modules; keep parser, DMA management, and Servo mapping out of `main.c`.
- Check and handle HAL return values. Represent driver outcomes with a named status enum rather than `-1`.

## Interrupts and callbacks

ISR and HAL callback work must remain short: capture events, update bounded state/indexes/counters, and perform required low-level recovery. Do not parse commands, format strings, move the Servo, call `HAL_Delay`, or do lengthy logging in an interrupt/callback.

Run receive consumption and command processing from the main loop. A DMA overrun invalidates the partial command; resume only after line resynchronization. UART/DMA error recovery must not spin forever or block for a long delay.

## Strings and protocol parsing

- Prefer explicit-length comparisons and a small decimal parser with complete error checks.
- Do not use `gets`, `strcpy`, `strcat`, or unbounded `sprintf`. Avoid `sscanf` for untrusted UART lines.
- Accept LF and CRLF per the protocol contract. Reject unknown commands, missing values, integer overflow, and out-of-range Servo inputs.
- Keep response strings and command grammar synchronized with `Docs/protocol.md`.

## CubeMX and CMake

- CubeMX owns `.ioc`, `Core/*`, and `cmake/stm32cubemx/*`; do not place driver logic in generated initialization or interrupt files.
- Preserve generated user-code regions and review code generation after peripheral changes.
- Add every source file and include directory explicitly in CMake. The CMake target is the build source of truth; do not rely on IDE source auto-discovery.
- Keep new compiler warnings at zero. Do not silence a warning by disabling diagnostics, adding an unjustified cast, or removing `const`.

## Scope boundary

The first milestone excludes a full TB6600/Stepper driver, acceleration profiles, Stepper DMA, K230 integration, FreeRTOS, and waste-sorting business logic. Preserve PA6/TIM3_CH1, PB12, PB13, and DMA1 Channel 6 for later work.

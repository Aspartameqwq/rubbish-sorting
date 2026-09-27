#ifndef PROJECT_CONFIG_H
#define PROJECT_CONFIG_H

/* =========================
 * HC-04 configuration
 * ========================= */

/* Test default; TO_BE_CONFIRMED against the actual HC-04 module and its peer. */
#define HC04_BAUDRATE                       115200U

#define HC04_RX_DMA_BUFFER_SIZE             256U
#define HC04_RX_RESTART_RETRY_MS             100U
#define HC04_TX_TIMEOUT_MS                  50U

/* =========================
 * Protocol configuration
 * ========================= */

#define PROTOCOL_COMMAND_BUFFER_SIZE        64U
#define PROTOCOL_MAX_BYTES_PER_PROCESS      64U

/* =========================
 * TB6600 configuration
 * ========================= */

/* 72 MHz APB1 timer clock / (71 + 1) = 1 MHz, 1 us per timer count. */
#define TB6600_TIMER_TICK_HZ                1000000U
#define TB6600_TIMER_PRESCALER              71U

/* INITIAL_SOFTWARE_VALUE; TO_BE_CONFIRMED against the actual driver module. */
#define TB6600_PULSE_HIGH_US                10U

/* INITIAL SOFTWARE LIMITS; TO_BE_CONFIRMED against the actual driver module. */
#define TB6600_STEP_FREQ_MIN_HZ             20U
#define TB6600_STEP_FREQ_MAX_HZ             10000U

/* SELECTED: direct 3.3 V common-cathode wiring; electrical behavior verification pending. */
#define TB6600_ENABLE_ACTIVE_LEVEL          1U
/* HIGH means logical FORWARD; verify actual mechanical orientation during bring-up. */
#define TB6600_DIR_FORWARD_LEVEL            1U
/* SELECTED active-high PUL polarity; pulse recognition/current verification pending. */
#define TB6600_PULSE_ACTIVE_LEVEL           1U

/* =========================
 * Stepper configuration
 * ========================= */

/* INITIAL_SOFTWARE_VALUE; TO_BE_CONFIRMED against the actual driver module. */
#define STEPPER_DIRECTION_SETUP_MS          1U

#endif /* PROJECT_CONFIG_H */

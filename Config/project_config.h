#ifndef PROJECT_CONFIG_H
#define PROJECT_CONFIG_H

/* =========================
 * Servo configuration
 * ========================= */

#define SERVO_MIN_ANGLE_DEG                 0U
#define SERVO_MAX_ANGLE_DEG                 270U
#define SERVO_CENTER_ANGLE_DEG              130U

/* INITIAL_TEST_VALUE window for bench checks; CALIBRATION REQUIRED for the actual Servo. */
#define SERVO_MIN_PULSE_US                  1400U
#define SERVO_CENTER_PULSE_US               1500U
#define SERVO_MAX_PULSE_US                  1600U
/* Logical-to-pulse mapping is a project selection; actual servo calibration is pending. */
#ifndef SERVO_CALIBRATION_VALID
#define SERVO_CALIBRATION_VALID             0U
#endif

/* Pitch software limits are placeholders until the mechanism is measured. */
#ifndef PITCH_SOFT_LIMIT_VALID
#define PITCH_SOFT_LIMIT_VALID              0U
#endif
#ifndef PITCH_SOFT_MIN_MDEG
#define PITCH_SOFT_MIN_MDEG                 0L
#endif
#ifndef PITCH_SOFT_MAX_MDEG
#define PITCH_SOFT_MAX_MDEG                 270000L
#endif

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

/* Selected from the module label for the reported 1.8 degree motor: 8 microsteps. */
#define YAW_PULSES_PER_REV                  1600U
#define ANGLE_MDEG_PER_REV                  360000L

/* Placeholder only; not calibrated to a physical yaw travel range. */
#ifndef YAW_SOFT_LIMIT_VALID
#define YAW_SOFT_LIMIT_VALID                0U
#endif
#ifndef YAW_SOFT_MIN_MDEG
#define YAW_SOFT_MIN_MDEG                   (-180000L)
#endif
#ifndef YAW_SOFT_MAX_MDEG
#define YAW_SOFT_MAX_MDEG                   180000L
#endif

/* Debug/Ozone settings. Control injection is enabled only by the Debug build target. */
#define DEBUG_SNAPSHOT_PERIOD_MS            20U
#define DEBUG_STATE_VERSION                 1U

#ifndef DEBUG_CONTROL_ENABLE
#define DEBUG_CONTROL_ENABLE                 0
#endif

#endif /* PROJECT_CONFIG_H */

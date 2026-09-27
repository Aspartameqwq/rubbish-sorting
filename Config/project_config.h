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
 * Four-box sorting configuration
 * ========================= */

/*
 * Mechanical angles and Pitch directions are deliberately uncommissioned.
 * Set these only after measuring the installed mechanism, then set
 * SORT_MECHANICAL_CALIBRATION_COMPLETE to 1. Sorting commands are rejected
 * while the flag is 0.
 */
#ifndef SORT_MECHANICAL_CALIBRATION_COMPLETE
#define SORT_MECHANICAL_CALIBRATION_COMPLETE 0U
#endif
#ifndef SORT_YAW_GROUP_13_MDEG
#define SORT_YAW_GROUP_13_MDEG               0L
#endif
#ifndef SORT_YAW_HOME_MDEG
#define SORT_YAW_HOME_MDEG                   0L
#endif
#ifndef SORT_YAW_GROUP_24_MDEG
#define SORT_YAW_GROUP_24_MDEG               0L
#endif
#ifndef SORT_PITCH_HOME_MDEG
#define SORT_PITCH_HOME_MDEG                 0L
#endif
#ifndef SORT_PITCH_DUMP_ANGLE_MDEG
#define SORT_PITCH_DUMP_ANGLE_MDEG           0L
#endif
#ifndef SORT_BOX1_PITCH_DIRECTION
#define SORT_BOX1_PITCH_DIRECTION             0
#endif
#ifndef SORT_BOX2_PITCH_DIRECTION
#define SORT_BOX2_PITCH_DIRECTION             0
#endif
#ifndef SORT_BOX3_PITCH_DIRECTION
#define SORT_BOX3_PITCH_DIRECTION             0
#endif
#ifndef SORT_BOX4_PITCH_DIRECTION
#define SORT_BOX4_PITCH_DIRECTION             0
#endif

#ifndef SORT_YAW_STEP_FREQUENCY_HZ
#define SORT_YAW_STEP_FREQUENCY_HZ            100U
#endif
#ifndef SORT_YAW_MOVE_TIMEOUT_MS
#define SORT_YAW_MOVE_TIMEOUT_MS              30000U
#endif
#ifndef SORT_YAW_MOVE_TIMEOUT_MARGIN_MS
#define SORT_YAW_MOVE_TIMEOUT_MARGIN_MS       5000U
#endif
#ifndef SORT_PITCH_MOVE_TIMEOUT_MS
#define SORT_PITCH_MOVE_TIMEOUT_MS            6000U
#endif
#ifndef SORT_DUMP_HOLD_MS
#define SORT_DUMP_HOLD_MS                     1000U
#endif
#ifndef SORT_READY_HEARTBEAT_MS
#define SORT_READY_HEARTBEAT_MS               1000U
#endif
#ifndef SORT_ACTION_HISTORY_CAPACITY
#define SORT_ACTION_HISTORY_CAPACITY           8U
#endif

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

/* Bench-observed module behavior: PB13 LOW enables holding torque; HIGH disables. */
#define TB6600_ENABLE_ACTIVE_LEVEL          0U
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

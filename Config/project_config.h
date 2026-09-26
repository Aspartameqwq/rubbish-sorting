#ifndef PROJECT_CONFIG_H
#define PROJECT_CONFIG_H

/*
 * Project-level configuration for the first STM32 firmware milestone.
 * Servo pulse values are conservative bench-test starting values only.
 * CALIBRATION REQUIRED before using the full mechanical range.
 */

#define SERVO_MIN_ANGLE_DEG                 0U
#define SERVO_MAX_ANGLE_DEG                 270U
#define SERVO_CENTER_ANGLE_DEG              135U

/* INITIAL_TEST_VALUE window for bench checks; CALIBRATION REQUIRED for the actual Servo. */
#define SERVO_MIN_PULSE_US                  1400U
#define SERVO_CENTER_PULSE_US               1500U
#define SERVO_MAX_PULSE_US                  1600U

/* Test default; TO_BE_CONFIRMED against the actual HC-04 module and its peer. */
#define HC04_BAUDRATE                       115200U

#define HC04_RX_DMA_BUFFER_SIZE             256U
#define HC04_RX_RESTART_RETRY_MS             100U
#define PROTOCOL_COMMAND_BUFFER_SIZE        64U
#define HC04_TX_TIMEOUT_MS                  50U

#endif /* PROJECT_CONFIG_H */

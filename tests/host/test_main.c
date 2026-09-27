#include "hc04.h"
#include "debug_state.h"
#include "control_debug_config.h"
#include "pitch_axis.h"
#include "project_config.h"
#include "protocol.h"
#include "servo.h"
#include "stepper.h"
#include "stepper_profile.h"
#include "tb6600.h"
#include "tb6600_timing.h"
#include "test_fakes.h"
#include "tim.h"
#include "yaw_axis.h"

#include <limits.h>
#include <stdio.h>
#include <string.h>

static unsigned int s_checks;
static unsigned int s_failures;

#define CHECK(expression)                                                        \
    do                                                                           \
    {                                                                            \
        s_checks++;                                                              \
        if (!(expression))                                                       \
        {                                                                        \
            (void)printf("FAIL %s:%d: %s\n", __FILE__, __LINE__, #expression);  \
            s_failures++;                                                        \
        }                                                                        \
    } while (0)

static void Test_TimingConversion(void)
{
    uint16_t auto_reload = 0U;
    uint16_t compare = 0U;

    CHECK(TB6600Timing_Calculate(1000000U, 20U, 10U, UINT16_MAX,
                                 &auto_reload, &compare) == TB6600_TIMING_STATUS_OK);
    CHECK(auto_reload == 49999U);
    CHECK(compare == 10U);

    CHECK(TB6600Timing_Calculate(1000000U, 10000U, 10U, UINT16_MAX,
                                 &auto_reload, &compare) == TB6600_TIMING_STATUS_OK);
    CHECK(auto_reload == 99U);
    CHECK(compare == 10U);

    CHECK(TB6600Timing_Calculate(1000000U, 0U, 10U, UINT16_MAX,
                                 &auto_reload, &compare) == TB6600_TIMING_STATUS_INVALID_ARGUMENT);
    CHECK(TB6600Timing_Calculate(1000000U, 1000001U, 10U, UINT16_MAX,
                                 &auto_reload, &compare) == TB6600_TIMING_STATUS_OUT_OF_RANGE);
    CHECK(TB6600Timing_Calculate(1000U, 100U, 9U, UINT16_MAX,
                                 &auto_reload, &compare) == TB6600_TIMING_STATUS_OUT_OF_RANGE);
    CHECK(TB6600Timing_Calculate(1000000U, 100U, 10U, 70000U,
                                 &auto_reload, &compare) == TB6600_TIMING_STATUS_INVALID_ARGUMENT);
    CHECK(TB6600Timing_Calculate(1000000U, 100U, 10U, UINT16_MAX,
                                 NULL, &compare) == TB6600_TIMING_STATUS_INVALID_ARGUMENT);
}

static void Test_StepperProfile(void)
{
    StepperProfile profile;
    StepperProfileConfig config = {1U, 20U, 1000U, 100U};
    uint32_t frequency;
    uint32_t index;

    CHECK(StepperProfile_Init(&profile, &config) == STEPPER_PROFILE_STATUS_OK);
    CHECK(profile.peak_frequency_hz == 20U);
    CHECK(StepperProfile_GetFrequencyAtStep(&profile, 0U, &frequency) == STEPPER_PROFILE_STATUS_OK);
    CHECK(frequency == 20U);
    CHECK(StepperProfile_GetFrequencyAtStep(&profile, 1U, &frequency) ==
          STEPPER_PROFILE_STATUS_OUT_OF_RANGE);

    config.total_steps = 2U;
    CHECK(StepperProfile_Init(&profile, &config) == STEPPER_PROFILE_STATUS_OK);
    CHECK(StepperProfile_GetFrequencyAtStep(&profile, 0U, &frequency) == STEPPER_PROFILE_STATUS_OK);
    CHECK(frequency == 20U);
    CHECK(StepperProfile_GetFrequencyAtStep(&profile, 1U, &frequency) == STEPPER_PROFILE_STATUS_OK);
    CHECK(frequency == 20U);

    config.total_steps = 100U;
    CHECK(StepperProfile_Init(&profile, &config) == STEPPER_PROFILE_STATUS_OK);
    CHECK(StepperProfile_GetFrequencyAtStep(&profile, 49U, &frequency) == STEPPER_PROFILE_STATUS_OK);
    CHECK(frequency == profile.peak_frequency_hz);
    CHECK(frequency <= config.maximum_frequency_hz);

    config.total_steps = 10000U;
    CHECK(StepperProfile_Init(&profile, &config) == STEPPER_PROFILE_STATUS_OK);
    CHECK(profile.peak_frequency_hz == config.maximum_frequency_hz);
    for (index = 0U; index < config.total_steps; index++)
    {
        uint32_t forward_frequency;
        uint32_t reverse_frequency;

        CHECK(StepperProfile_GetFrequencyAtStep(&profile, index, &forward_frequency) ==
              STEPPER_PROFILE_STATUS_OK);
        CHECK(StepperProfile_GetFrequencyAtStep(&profile,
                                                config.total_steps - 1U - index,
                                                &reverse_frequency) == STEPPER_PROFILE_STATUS_OK);
        CHECK(forward_frequency == reverse_frequency);
        CHECK((forward_frequency >= config.start_frequency_hz) &&
              (forward_frequency <= config.maximum_frequency_hz));
    }

    config.total_steps = UINT32_MAX;
    config.start_frequency_hz = 1U;
    config.maximum_frequency_hz = UINT32_MAX;
    config.acceleration_steps_per_second_squared = UINT32_MAX;
    CHECK(StepperProfile_Init(&profile, &config) == STEPPER_PROFILE_STATUS_OK);
    CHECK(StepperProfile_GetFrequencyAtStep(&profile,
                                            (config.total_steps - 1U) / 2U,
                                            &frequency) == STEPPER_PROFILE_STATUS_OK);
    CHECK((frequency >= config.start_frequency_hz) &&
          (frequency <= config.maximum_frequency_hz));

    config.acceleration_steps_per_second_squared = 0U;
    CHECK(StepperProfile_Init(&profile, &config) == STEPPER_PROFILE_STATUS_INVALID_ARGUMENT);
    CHECK(StepperProfile_Init(NULL, &config) == STEPPER_PROFILE_STATUS_INVALID_ARGUMENT);
    CHECK(StepperProfile_Init(&profile, NULL) == STEPPER_PROFILE_STATUS_INVALID_ARGUMENT);
}

static void Test_InitializeModules(void)
{
    CHECK(Servo_Init() == SERVO_STATUS_OK);
    CHECK(TB6600_Init() == TB6600_STATUS_OK);
    CHECK(!TB6600_IsEnabled());
    CHECK(!TB6600_IsPulseRunning());
    CHECK(TestFakes_GetPwmStartCount() == 0U);
    CHECK(TestFakes_IsTim3PwmConfigured());
    CHECK(TestFakes_GetTim3PwmPolarity() == TIM_OCPOLARITY_HIGH);
    CHECK(TestFakes_GetGpioState(TB6600_ENA_GPIO_Port, TB6600_ENA_Pin) == GPIO_PIN_RESET);
    CHECK(TestFakes_GetGpioState(TB6600_DIR_GPIO_Port, TB6600_DIR_Pin) == GPIO_PIN_RESET);

    CHECK(TB6600_Enable() == TB6600_STATUS_OK);
    CHECK(TestFakes_GetGpioState(TB6600_ENA_GPIO_Port, TB6600_ENA_Pin) == GPIO_PIN_SET);
    CHECK(TB6600_Disable() == TB6600_STATUS_OK);
    CHECK(TestFakes_GetGpioState(TB6600_ENA_GPIO_Port, TB6600_ENA_Pin) == GPIO_PIN_RESET);
    CHECK(TB6600_SetDirection(TB6600_DIRECTION_FORWARD) == TB6600_STATUS_OK);
    CHECK(TestFakes_GetGpioState(TB6600_DIR_GPIO_Port, TB6600_DIR_Pin) == GPIO_PIN_SET);
    CHECK(TB6600_SetDirection(TB6600_DIRECTION_REVERSE) == TB6600_STATUS_OK);
    CHECK(TestFakes_GetGpioState(TB6600_DIR_GPIO_Port, TB6600_DIR_Pin) == GPIO_PIN_RESET);

    CHECK(Stepper_Init() == STEPPER_STATUS_OK);
    CHECK(!Stepper_IsEnabled());
    CHECK(PitchAxis_Init() == PITCH_AXIS_STATUS_OK);
    CHECK(YawAxis_Init() == YAW_AXIS_STATUS_OK);
    Protocol_Init();
    Debug_Init();
    TestFakes_ResetUart();
    TestFakes_SetTick(100U);
}

static void Test_FractionalServoCenter(void)
{
#if (PITCH_LEVEL_SERVO_MDEG == 130500L)
    uint16_t pulse_us = 0U;
    int32_t angle_mdeg = INT32_MIN;

    CHECK(SERVO_CENTER_ANGLE_MDEG == 130500L);
    CHECK(SERVO_CENTER_ANGLE_DEG == 130L);
    CHECK(PitchAxis_Init() == PITCH_AXIS_STATUS_OK);
    CHECK(PitchAxis_GetTargetMilliDeg() == 0);
    CHECK(PitchAxis_GetServoTargetMilliDeg() == 130500L);
    CHECK(PitchAxis_GetPulseUs() == SERVO_CENTER_PULSE_US);
    CHECK(Servo_ConvertAngleMilliDegToPulseUs(130500L, &pulse_us) == SERVO_STATUS_OK);
    CHECK(pulse_us == SERVO_CENTER_PULSE_US);
    CHECK(Servo_ConvertPulseUsToAngleMilliDeg(pulse_us, &angle_mdeg) == SERVO_STATUS_OK);
    CHECK(angle_mdeg == 130500L);
#endif
}

static void Test_ServoStateAndProtocol(void)
{
    CHECK(Servo_IsAngleValid());
    CHECK(Servo_GetAngle() == SERVO_CENTER_ANGLE_DEG);
    CHECK(Servo_SetAngle(SERVO_MIN_ANGLE_DEG) == SERVO_STATUS_OK);
    CHECK(Servo_GetAngle() == SERVO_MIN_ANGLE_DEG);
    CHECK(Servo_SetAngle(SERVO_CENTER_ANGLE_DEG) == SERVO_STATUS_OK);
    CHECK(Servo_GetAngle() == SERVO_CENTER_ANGLE_DEG);
    CHECK(Servo_SetAngle(SERVO_MAX_ANGLE_DEG) == SERVO_STATUS_OK);
    CHECK(Servo_GetAngle() == SERVO_MAX_ANGLE_DEG);
    CHECK(Servo_SetAngle((uint16_t)(SERVO_MAX_ANGLE_DEG + 1U)) == SERVO_STATUS_INVALID_ARGUMENT);
    CHECK(Servo_SetPulseUs(1510U) == SERVO_STATUS_OK);
    CHECK(!Servo_IsAngleValid());
    CHECK(Servo_GetPulseUs() == 1510U);

    CHECK(PitchAxis_Init() == PITCH_AXIS_STATUS_OK);
    CHECK(PitchAxis_GetTargetMilliDeg() == 0);
    CHECK(PitchAxis_GetCommandedMilliDeg() == 0);
    CHECK(PitchAxis_GetServoTargetMilliDeg() == PITCH_LEVEL_SERVO_MDEG);
    CHECK(PitchAxis_GetPulseUs() == SERVO_CENTER_PULSE_US);

    TestFakes_FeedUart("PING\r\n");
    Protocol_Process();
    CHECK(strcmp(TestFakes_TxData(), "PONG\r\n") == 0);
    TestFakes_ClearTx();

    TestFakes_FeedUart("PITCH 0\nPITCH +5000\nPITCH -10000\n");
    Protocol_Process();
    CHECK(strcmp(TestFakes_TxData(), "OK\r\nOK\r\nOK\r\n") == 0);
    CHECK(PitchAxis_GetTargetMilliDeg() == -10000);
    TestFakes_ClearTx();

    TestFakes_FeedUart("PITCH 30000\nPITCH 30001\nPITCH -30000\nPITCH -30001\n");
    Protocol_Process();
    CHECK(strcmp(TestFakes_TxData(), "OK\r\nERR\r\nOK\r\nERR\r\n") == 0);
    CHECK(PitchAxis_GetTargetMilliDeg() == -30000);
    TestFakes_ClearTx();

    TestFakes_FeedUart("PITCH 2147483648\nPITCH -2147483649\nPITCH 10 extra\n");
    Protocol_Process();
    CHECK(strcmp(TestFakes_TxData(), "ERR\r\nERR\r\nERR\r\n") == 0);
    TestFakes_ClearTx();

#if (RAW_BENCH_COMMANDS_ENABLE == 1)
    TestFakes_FeedUart("SERVO 130\nSERVO?\nSERVO 99\nSERVO_US 1400\nSERVO_US 1510\nSERVO?\n");
    Protocol_Process();
    CHECK(strcmp(TestFakes_TxData(),
                 "OK\r\nSERVO 130\r\nERR\r\nERR\r\nOK\r\nSERVO RAW 1510\r\n") == 0);
    TestFakes_ClearTx();
    TestFakes_FeedUart("SERVO 65535\n");
    Protocol_Process();
    CHECK(strcmp(TestFakes_TxData(), "ERR\r\n") == 0);
    CHECK(PitchAxis_IsRawPulseMode());
    CHECK(PitchAxis_GetTargetMilliDeg() == INT32_MIN);
#else
    TestFakes_FeedUart("SERVO 130\nSERVO_US 1510\nSERVO?\n");
    Protocol_Process();
    CHECK(strcmp(TestFakes_TxData(), "ERR\r\nERR\r\nSERVO 130\r\n") == 0);
    CHECK(!PitchAxis_IsRawPulseMode());
#endif
}

static void Test_RunFiniteMove(uint32_t pulse_count,
                               int32_t signed_steps,
                               uint32_t frequency_hz,
                               int32_t expected_position)
{
    uint32_t pulse_index;
    uint32_t now;

    CHECK(Stepper_MoveSteps(signed_steps, frequency_hz) == STEPPER_STATUS_OK);
    now = TestFakes_GetTick();
    TestFakes_SetTick(now + STEPPER_DIRECTION_SETUP_MS);
    Stepper_Process();
    CHECK(Stepper_GetState() == STEPPER_STATE_RUNNING);
    CHECK(TB6600_GetCompletedPulseCount() == 0U);

    for (pulse_index = 0U; pulse_index < pulse_count; pulse_index++)
    {
        HAL_TIM_PWM_PulseFinishedCallback(&htim3);
        CHECK(TB6600_GetCompletedPulseCount() == (pulse_index + 1U));
        CHECK(TB6600_IsPulseRunning() == ((pulse_index + 1U) < pulse_count));
    }
    HAL_TIM_PWM_PulseFinishedCallback(&htim3);
    CHECK(TB6600_GetCompletedPulseCount() == pulse_count);

    Stepper_Process();
    CHECK(Stepper_GetState() == STEPPER_STATE_IDLE);
    CHECK(Stepper_GetCommandedPosition() == expected_position);
    CHECK(Stepper_GetRemainingSteps() == 0U);
}

static void Test_StepperFiniteMoves(void)
{
    uint32_t starts_before;
    uint32_t stops_before;

    CHECK(Stepper_Init() == STEPPER_STATUS_OK);
    CHECK(Stepper_GetState() == STEPPER_STATE_DISABLED);
    CHECK(Stepper_Enable() == STEPPER_STATUS_OK);
    CHECK(Stepper_MoveSteps(1, 20U) == STEPPER_STATUS_OK);
    CHECK(Stepper_GetState() == STEPPER_STATE_DIRECTION_SETUP);
    starts_before = TestFakes_GetPwmStartCount();
    Stepper_Process();
    CHECK(TestFakes_GetPwmStartCount() == starts_before);
    TestFakes_SetTick(101U);
    Stepper_Process();
    CHECK(Stepper_GetState() == STEPPER_STATE_RUNNING);
    CHECK(TB6600_IsPulseRunning());
    CHECK(htim3.autoreload == 49999U);
    CHECK(htim3.compare1 == 10U);

    stops_before = TestFakes_GetPwmStopCount();
    HAL_TIM_PWM_PulseFinishedCallback(&htim3);
    CHECK(!TB6600_IsPulseRunning());
    CHECK(TB6600_GetCompletedPulseCount() == 1U);
    HAL_TIM_PWM_PulseFinishedCallback(&htim3);
    CHECK(TB6600_GetCompletedPulseCount() == 1U);
    Stepper_Process();
    CHECK(!Stepper_IsBusy());
    CHECK(Stepper_GetCommandedPosition() == 1);
    CHECK(Stepper_GetRemainingSteps() == 0U);
    CHECK(TestFakes_GetPwmStopCount() == (stops_before + 1U));

    CHECK(Stepper_MoveSteps(-2, 10000U) == STEPPER_STATUS_OK);
    TestFakes_SetTick(102U);
    Stepper_Process();
    CHECK(htim3.autoreload == 99U);
    CHECK(Stepper_GetState() == STEPPER_STATE_RUNNING);
    HAL_TIM_PWM_PulseFinishedCallback(&htim3);
    Stepper_Process();
    CHECK(Stepper_GetCommandedPosition() == 0);
    CHECK(Stepper_GetRemainingSteps() == 1U);
    CHECK(TB6600_IsPulseRunning());
    HAL_TIM_PWM_PulseFinishedCallback(&htim3);
    Stepper_Process();
    CHECK(Stepper_GetCommandedPosition() == -1);
    CHECK(Stepper_GetRemainingSteps() == 0U);
    CHECK(Stepper_GetState() == STEPPER_STATE_IDLE);

    Test_RunFiniteMove(10U, 10, 1000U, 9);
    Test_RunFiniteMove(100U, -100, 200U, -91);
    Test_RunFiniteMove(90U, 90, 500U, -1);

    CHECK(Stepper_MoveSteps(100, 1000U) == STEPPER_STATUS_OK);
    TestFakes_SetTick(TestFakes_GetTick() + STEPPER_DIRECTION_SETUP_MS);
    Stepper_Process();
    CHECK(Stepper_Stop() == STEPPER_STATUS_OK);
    CHECK(Stepper_GetState() == STEPPER_STATE_STOPPING);
    HAL_TIM_PWM_PulseFinishedCallback(&htim3);
    CHECK(!TB6600_IsPulseRunning());
    Stepper_Process();
    CHECK(Stepper_GetCommandedPosition() == 0);
    CHECK(Stepper_GetState() == STEPPER_STATE_IDLE);

    CHECK(Stepper_MoveSteps(4, 200U) == STEPPER_STATUS_OK);
    TestFakes_SetTick(TestFakes_GetTick() + STEPPER_DIRECTION_SETUP_MS);
    Stepper_Process();
    CHECK(Stepper_Disable() == STEPPER_STATUS_OK);
    CHECK(Stepper_GetState() == STEPPER_STATE_STOPPING);
    HAL_TIM_PWM_PulseFinishedCallback(&htim3);
    Stepper_Process();
    CHECK(Stepper_GetState() == STEPPER_STATE_DISABLED);
    CHECK(!TB6600_IsEnabled());

    CHECK(Stepper_Init() == STEPPER_STATUS_OK);
    CHECK(Stepper_Enable() == STEPPER_STATUS_OK);
    CHECK(Stepper_MoveSteps(INT32_MIN, TB6600_STEP_FREQ_MIN_HZ) == STEPPER_STATUS_OK);
    CHECK(Stepper_GetRemainingSteps() == 2147483648U);
    CHECK(Stepper_EmergencyStop() == STEPPER_STATUS_OK);
    CHECK(Stepper_GetCommandedPosition() == 0);
    CHECK(Stepper_GetState() == STEPPER_STATE_IDLE);
    CHECK(Stepper_EmergencyStop() == STEPPER_STATUS_OK);
    CHECK(Stepper_GetCommandedPosition() == 0);

    CHECK(Stepper_MoveSteps(1, 0U) == STEPPER_STATUS_INVALID_ARGUMENT);
    CHECK(Stepper_MoveSteps(0, 100U) == STEPPER_STATUS_OK);
    CHECK(Stepper_MoveSteps(1, TB6600_STEP_FREQ_MAX_HZ + 1U) == STEPPER_STATUS_INVALID_ARGUMENT);
}

static void Test_ProtocolStepperAndBounds(void)
{
    char oversized[80];
    size_t index;

    CHECK(Stepper_Init() == STEPPER_STATUS_OK);
    CHECK(YawAxis_Init() == YAW_AXIS_STATUS_OK);
    TestFakes_ResetUart();
    Protocol_Init();

    TestFakes_FeedUart("STEPPER?\n");
    Protocol_Process();
    CHECK(strcmp(TestFakes_TxData(), "STEPPER DISABLED POS=0 REM=0 FREQ=0\r\n") == 0);
    TestFakes_ClearTx();

#if (RAW_BENCH_COMMANDS_ENABLE == 1)
    TestFakes_FeedUart("STEPPER ENABLE\nSTEPPER MOVE 1 20\n");
    Protocol_Process();
    CHECK(strcmp(TestFakes_TxData(), "OK\r\nERR\r\n") == 0);
    CHECK(Stepper_GetState() == STEPPER_STATE_IDLE);
    CHECK(Stepper_GetCommandedPosition() == 0);
    CHECK(YawAxis_GetLastStatus() == YAW_AXIS_STATUS_NOT_REFERENCED);
    TestFakes_ClearTx();
    TestFakes_FeedUart("STEPPER DISABLE\n");
    Protocol_Process();
    CHECK(strcmp(TestFakes_TxData(), "OK\r\n") == 0);
    CHECK(YawAxis_GetReferenceState() == YAW_REFERENCE_INVALID);
    TestFakes_ClearTx();
    CHECK(YawAxis_SetCurrentPositionAsZero() == YAW_AXIS_STATUS_OK);
#endif

    TestFakes_FeedUart("STEPPER ENABLE\nSTEPPER MOVE 3 500\n");
    Protocol_Process();
#if (RAW_BENCH_COMMANDS_ENABLE == 1)
    CHECK(strcmp(TestFakes_TxData(), "OK\r\nOK\r\n") == 0);
    CHECK(Stepper_GetState() == STEPPER_STATE_DIRECTION_SETUP);
    TestFakes_SetTick(105U);
    YawAxis_Process();
    CHECK(Stepper_GetState() == STEPPER_STATE_RUNNING);
    HAL_TIM_PWM_PulseFinishedCallback(&htim3);
    HAL_TIM_PWM_PulseFinishedCallback(&htim3);
    CHECK(TB6600_IsPulseRunning());
    HAL_TIM_PWM_PulseFinishedCallback(&htim3);
    CHECK(!TB6600_IsPulseRunning());
    YawAxis_Process();
    CHECK(Stepper_GetCommandedPosition() == 3);
    CHECK(Stepper_GetState() == STEPPER_STATE_IDLE);
#else
    CHECK(strcmp(TestFakes_TxData(), "ERR\r\nERR\r\n") == 0);
    CHECK(Stepper_GetState() == STEPPER_STATE_DISABLED);
#endif
    TestFakes_ClearTx();
#if (RAW_BENCH_COMMANDS_ENABLE == 1)
    TestFakes_FeedUart("STEPPER STOP\n");
    Protocol_Process();
    CHECK(strcmp(TestFakes_TxData(), "OK\r\n") == 0);
    CHECK(YawAxis_GetReferenceState() == YAW_REFERENCE_MANUAL);
    TestFakes_ClearTx();
    TestFakes_FeedUart("STEPPER MOVE 1 501\n");
    Protocol_Process();
    CHECK(strcmp(TestFakes_TxData(), "ERR\r\n") == 0);
    CHECK(Stepper_GetCommandedPosition() == 3);
    CHECK(Stepper_GetRemainingSteps() == 0U);
    TestFakes_ClearTx();
#endif

    TestFakes_FeedUart("STEPPER?\n");
    Protocol_Process();
#if (RAW_BENCH_COMMANDS_ENABLE == 1)
    CHECK(strcmp(TestFakes_TxData(), "STEPPER IDLE POS=3 REM=0 FREQ=0\r\n") == 0);
#else
    CHECK(strcmp(TestFakes_TxData(), "STEPPER DISABLED POS=0 REM=0 FREQ=0\r\n") == 0);
#endif
    TestFakes_ClearTx();

    TestFakes_FeedUart("STEPPER MOVE -2147483648 20\n");
    Protocol_Process();
#if (RAW_BENCH_COMMANDS_ENABLE == 1)
    CHECK(strcmp(TestFakes_TxData(), "ERR\r\n") == 0);
    CHECK(Stepper_GetRemainingSteps() == 0U);
    CHECK(Stepper_GetCommandedPosition() == 3);
#else
    CHECK(strcmp(TestFakes_TxData(), "ERR\r\n") == 0);
#endif
    TestFakes_ClearTx();

    TestFakes_FeedUart("STEPPER MOVE 2147483648 20\n"
                       "STEPPER MOVE -2147483649 20\n"
                       "STEPPER MOVE 1 0\n"
                       "STEPPER MOVE 1 10001\n"
                       "STEPPER MOVE 1 20 EXTRA\n"
                       "STEPPER MOVE 1\n"
                       "STEPPER MOVE 1 20 30\n");
    while (TestFakes_UartPending() != 0U)
    {
        Protocol_Process();
    }
    CHECK(strcmp(TestFakes_TxData(),
                 "ERR\r\nERR\r\nERR\r\nERR\r\nERR\r\nERR\r\nERR\r\n") == 0);
    TestFakes_ClearTx();

    TestFakes_FeedUart("STEPPER DISABLE\nSTEPPER ENABLE\nSTEPPER STOP\n");
    Protocol_Process();
#if (RAW_BENCH_COMMANDS_ENABLE == 1)
    CHECK(strcmp(TestFakes_TxData(), "OK\r\nOK\r\nOK\r\n") == 0);
    CHECK(YawAxis_GetReferenceState() == YAW_REFERENCE_INVALID);
#else
    CHECK(strcmp(TestFakes_TxData(), "OK\r\nERR\r\nOK\r\n") == 0);
#endif
    TestFakes_ClearTx();

    TestFakes_FeedUart("STEPPER MOVE 1 20\n");
    Protocol_Process();
    CHECK(strcmp(TestFakes_TxData(), "ERR\r\n") == 0);
#if (RAW_BENCH_COMMANDS_ENABLE == 1)
    CHECK(Stepper_GetCommandedPosition() == 3);
#else
    CHECK(Stepper_GetCommandedPosition() == 0);
#endif
    CHECK(Stepper_GetRemainingSteps() == 0U);
    TestFakes_ClearTx();

    for (index = 0U; index < 64U; index++)
    {
        oversized[index] = 'X';
    }
    oversized[64] = '\n';
    (void)memcpy(&oversized[65], "PING\n", 6U);
    TestFakes_FeedUart(oversized);
    Protocol_Process();
    CHECK(TestFakes_UartPending() == 6U);
    CHECK(TestFakes_TxData()[0] == '\0');
    Protocol_Process();
    CHECK(strcmp(TestFakes_TxData(), "ERR\r\nPONG\r\n") == 0);
    CHECK(TestFakes_UartPending() == 0U);
}

static void Test_CompleteYawTarget(int32_t target_mdeg,
                                   int32_t expected_relative_pulses,
                                   int32_t expected_quantized_mdeg)
{
    int32_t absolute_target;
    int32_t delta;
    uint32_t pulse_count;
    uint32_t pulse_index;
    uint32_t now;

    CHECK(YawAxis_SetTargetMilliDeg(target_mdeg, 20U) == YAW_AXIS_STATUS_OK);
    CHECK(YawAxis_GetTargetMilliDeg() == target_mdeg);
    CHECK(YawAxis_GetQuantizedTargetMilliDeg() == expected_quantized_mdeg);

    absolute_target = YawAxis_GetZeroOffsetPulses() + expected_relative_pulses;
    delta = absolute_target - Stepper_GetCommandedPosition();
    pulse_count = (delta < 0) ? (uint32_t)(-(int64_t)delta) : (uint32_t)delta;
    if (pulse_count == 0U)
    {
        CHECK(YawAxis_GetCommandedMilliDeg() == expected_quantized_mdeg);
        return;
    }

    now = TestFakes_GetTick();
    TestFakes_SetTick(now + STEPPER_DIRECTION_SETUP_MS);
    YawAxis_Process();
    CHECK(Stepper_GetState() == STEPPER_STATE_RUNNING);
    for (pulse_index = 0U; pulse_index < pulse_count; pulse_index++)
    {
        HAL_TIM_PWM_PulseFinishedCallback(&htim3);
    }
    YawAxis_Process();
    CHECK(!YawAxis_IsBusy());
    CHECK(Stepper_GetCommandedPosition() == absolute_target);
    CHECK(YawAxis_GetCommandedMilliDeg() == expected_quantized_mdeg);
}

static void Test_CompleteYawRelativeMove(int32_t delta_pulses)
{
    const int32_t initial_position = Stepper_GetCommandedPosition();
    const uint32_t pulse_count = (delta_pulses < 0)
                                     ? (uint32_t)(-(int64_t)delta_pulses)
                                     : (uint32_t)delta_pulses;
    const int32_t expected_position = initial_position + delta_pulses;
    const int64_t relative_pulses = (int64_t)expected_position -
                                    YawAxis_GetZeroOffsetPulses();
    const int32_t expected_mdeg = (int32_t)(relative_pulses *
                                            (ANGLE_MDEG_PER_REV /
                                             YAW_AXIS_PULSES_PER_REV));
    uint32_t pulse_index;

    CHECK(YawAxis_MoveRelativePulses(delta_pulses, YAW_STEP_FREQ_MIN_HZ) ==
          YAW_AXIS_STATUS_OK);
    if (pulse_count == 0U)
    {
        CHECK(!YawAxis_IsBusy());
        return;
    }

    TestFakes_SetTick(TestFakes_GetTick() + STEPPER_DIRECTION_SETUP_MS);
    YawAxis_Process();
    CHECK(Stepper_GetState() == STEPPER_STATE_RUNNING);
    for (pulse_index = 0U; pulse_index < pulse_count; pulse_index++)
    {
        HAL_TIM_PWM_PulseFinishedCallback(&htim3);
    }
    YawAxis_Process();
    CHECK(Stepper_GetCommandedPosition() == expected_position);
    CHECK(YawAxis_GetCommandedMilliDeg() == expected_mdeg);
}

static void Test_YawReferenceLifecycle(void)
{
    uint32_t pulse_index;

    CHECK(Stepper_Init() == STEPPER_STATUS_OK);
    CHECK(YawAxis_Init() == YAW_AXIS_STATUS_OK);
    CHECK(YawAxis_GetReferenceState() == YAW_REFERENCE_INVALID);
    CHECK(YawAxis_GetTargetMilliDeg() == INT32_MIN);
    CHECK(YawAxis_GetQuantizedTargetMilliDeg() == INT32_MIN);
    CHECK(YawAxis_GetCommandedMilliDeg() == INT32_MIN);
    CHECK(YawAxis_GetCableMarginToMinMilliDeg() == INT32_MIN);
    CHECK(YawAxis_GetCableMarginToMaxMilliDeg() == INT32_MIN);
    CHECK(YawAxis_GetCableRemainingNegativePulses() == 0U);
    CHECK(YawAxis_GetCableRemainingPositivePulses() == 0U);
    CHECK(YawAxis_SetTargetMilliDeg(5000, 100U) == YAW_AXIS_STATUS_NOT_REFERENCED);
    CHECK(YawAxis_MoveRelativePulses(1, 100U) == YAW_AXIS_STATUS_NOT_REFERENCED);

    CHECK(YawAxis_SetCurrentPositionAsZero() == YAW_AXIS_STATUS_OK);
    CHECK(YawAxis_GetReferenceState() == YAW_REFERENCE_MANUAL);
    CHECK(YawAxis_Enable() == YAW_AXIS_STATUS_OK);
    CHECK(YawAxis_SetCurrentPositionAsZero() == YAW_AXIS_STATUS_INVALID_STATE);

    CHECK(YawAxis_MoveRelativePulses(2, 100U) == YAW_AXIS_STATUS_OK);
    CHECK(YawAxis_SetCurrentPositionAsZero() == YAW_AXIS_STATUS_INVALID_STATE);
    TestFakes_SetTick(TestFakes_GetTick() + STEPPER_DIRECTION_SETUP_MS);
    YawAxis_Process();
    CHECK(Stepper_GetState() == STEPPER_STATE_RUNNING);
    CHECK(YawAxis_SetCurrentPositionAsZero() == YAW_AXIS_STATUS_INVALID_STATE);
    HAL_TIM_PWM_PulseFinishedCallback(&htim3);
    YawAxis_Process();
    CHECK(YawAxis_Stop() == YAW_AXIS_STATUS_OK);
    CHECK(Stepper_GetState() == STEPPER_STATE_STOPPING);
    CHECK(YawAxis_SetCurrentPositionAsZero() == YAW_AXIS_STATUS_INVALID_STATE);
    HAL_TIM_PWM_PulseFinishedCallback(&htim3);
    YawAxis_Process();
    CHECK(Stepper_GetState() == STEPPER_STATE_IDLE);
    CHECK(YawAxis_GetReferenceState() == YAW_REFERENCE_MANUAL);
    CHECK(YawAxis_GetCommandedMilliDeg() == 450);
    CHECK(YawAxis_SetCurrentPositionAsZero() == YAW_AXIS_STATUS_INVALID_STATE);

    CHECK(YawAxis_Disable() == YAW_AXIS_STATUS_OK);
    CHECK(Stepper_GetState() == STEPPER_STATE_DISABLED);
    CHECK(YawAxis_GetReferenceState() == YAW_REFERENCE_INVALID);
    CHECK(YawAxis_GetTargetMilliDeg() == INT32_MIN);
    CHECK(YawAxis_GetQuantizedTargetMilliDeg() == INT32_MIN);
    CHECK(YawAxis_GetCommandedMilliDeg() == INT32_MIN);
    CHECK(YawAxis_GetCableMarginToMinMilliDeg() == INT32_MIN);
    CHECK(YawAxis_GetCableMarginToMaxMilliDeg() == INT32_MIN);
    CHECK(YawAxis_Enable() == YAW_AXIS_STATUS_OK);
    CHECK(YawAxis_SetTargetMilliDeg(0, 100U) == YAW_AXIS_STATUS_NOT_REFERENCED);
    CHECK(YawAxis_MoveRelativePulses(-1, 100U) == YAW_AXIS_STATUS_NOT_REFERENCED);
    CHECK(YawAxis_Disable() == YAW_AXIS_STATUS_OK);

    CHECK(YawAxis_SetCurrentPositionAsZero() == YAW_AXIS_STATUS_OK);
    CHECK(YawAxis_GetZeroOffsetPulses() == 2);
    CHECK(YawAxis_GetCommandedMilliDeg() == 0);
    CHECK(YawAxis_Enable() == YAW_AXIS_STATUS_OK);
    CHECK(YawAxis_MoveRelativePulses(-2, 100U) == YAW_AXIS_STATUS_OK);
    TestFakes_SetTick(TestFakes_GetTick() + STEPPER_DIRECTION_SETUP_MS);
    YawAxis_Process();
    for (pulse_index = 0U; pulse_index < 2U; pulse_index++)
    {
        HAL_TIM_PWM_PulseFinishedCallback(&htim3);
    }
    YawAxis_Process();
    CHECK(YawAxis_GetReferenceState() == YAW_REFERENCE_MANUAL);
    CHECK(YawAxis_Disable() == YAW_AXIS_STATUS_OK);
    CHECK(YawAxis_GetReferenceState() == YAW_REFERENCE_INVALID);
}

static void Test_YawCablePulseBounds(void)
{
    uint32_t rejects;

    CHECK(Stepper_Init() == STEPPER_STATUS_OK);
    CHECK(YawAxis_Init() == YAW_AXIS_STATUS_OK);
    CHECK(YawAxis_SetCurrentPositionAsZero() == YAW_AXIS_STATUS_OK);
    CHECK(YawAxis_Enable() == YAW_AXIS_STATUS_OK);

    Test_CompleteYawRelativeMove(700);
    rejects = YawAxis_GetLimitRejectCount();
    CHECK(YawAxis_MoveRelativePulses(101, 20U) == YAW_AXIS_STATUS_LIMIT);
    CHECK(YawAxis_GetLimitRejectCount() == rejects + 1U);
    CHECK(Stepper_GetCommandedPosition() == 700);
    Test_CompleteYawRelativeMove(-100);
    Test_CompleteYawRelativeMove(100);
    Test_CompleteYawRelativeMove(100);
    CHECK(Stepper_GetCommandedPosition() == 800);
    CHECK(YawAxis_MoveRelativePulses(1, 20U) == YAW_AXIS_STATUS_LIMIT);
    CHECK(YawAxis_GetCableMarginToMaxMilliDeg() == 0);
    CHECK(YawAxis_GetCableMarginToMinMilliDeg() == 360000);
    CHECK(YawAxis_GetCableRemainingPositivePulses() == 0U);
    CHECK(YawAxis_GetCableRemainingNegativePulses() == 1600U);

    Test_CompleteYawRelativeMove(-1500);
    CHECK(Stepper_GetCommandedPosition() == -700);
    rejects = YawAxis_GetLimitRejectCount();
    CHECK(YawAxis_MoveRelativePulses(-101, 20U) == YAW_AXIS_STATUS_LIMIT);
    CHECK(YawAxis_GetLimitRejectCount() == rejects + 1U);
    Test_CompleteYawRelativeMove(100);
    Test_CompleteYawRelativeMove(-100);
    Test_CompleteYawRelativeMove(-100);
    CHECK(Stepper_GetCommandedPosition() == -800);
    CHECK(YawAxis_MoveRelativePulses(-1, 20U) == YAW_AXIS_STATUS_LIMIT);
    CHECK(YawAxis_GetCableMarginToMinMilliDeg() == 0);
    CHECK(YawAxis_GetCableMarginToMaxMilliDeg() == 360000);
    CHECK(YawAxis_GetCableRemainingNegativePulses() == 0U);
    CHECK(YawAxis_GetCableRemainingPositivePulses() == 1600U);
}

static void Test_YawNoShortestPath(void)
{
    CHECK(Stepper_Init() == STEPPER_STATUS_OK);
    CHECK(YawAxis_Init() == YAW_AXIS_STATUS_OK);
    CHECK(YawAxis_SetCurrentPositionAsZero() == YAW_AXIS_STATUS_OK);
    CHECK(YawAxis_Enable() == YAW_AXIS_STATUS_OK);

    Test_CompleteYawTarget(170000, 756, 170100);
    CHECK(YawAxis_GetCommandedMilliDeg() == 170100);
    CHECK(YawAxis_SetTargetMilliDeg(-170000, 100U) == YAW_AXIS_STATUS_OK);
    CHECK(YawAxis_GetTargetMilliDeg() == -170000);
    CHECK(YawAxis_GetQuantizedTargetMilliDeg() == -170100);
    CHECK(Stepper_GetRemainingSteps() == 1512U);
    CHECK(TestFakes_GetGpioState(TB6600_DIR_GPIO_Port, TB6600_DIR_Pin) ==
          ((TB6600_DIR_FORWARD_LEVEL != 0U) ? GPIO_PIN_RESET : GPIO_PIN_SET));
    CHECK(YawAxis_Stop() == YAW_AXIS_STATUS_OK);
    CHECK(YawAxis_GetReferenceState() == YAW_REFERENCE_MANUAL);
}

static void Test_YawAxisScaleAndFrequencyPolicy(void)
{
    CHECK(DEBUG_STATE_VERSION == 4U);
    CHECK(DEBUG_CMD_NONE == 0);
    CHECK(DEBUG_CMD_SET_PITCH_MDEG == 1);
    CHECK(DEBUG_CMD_SET_PITCH_PULSE_US == 2);
    CHECK(DEBUG_CMD_SET_YAW_MDEG == 3);
    CHECK(DEBUG_CMD_SET_BOTH_MDEG == 4);
    CHECK(DEBUG_CMD_SET_YAW_ZERO == 5);
    CHECK(DEBUG_CMD_YAW_ENABLE == 6);
    CHECK(DEBUG_CMD_YAW_DISABLE == 7);
    CHECK(DEBUG_CMD_YAW_STOP == 8);
    CHECK(DEBUG_CMD_SET_PITCH_RESPONSE_MS == 9);
    CHECK(YAW_AXIS_PULSES_PER_REV == 1600U);
    CHECK(YAW_PULSES_PER_REV == YAW_AXIS_PULSES_PER_REV);
    CHECK((ANGLE_MDEG_PER_REV / YAW_AXIS_PULSES_PER_REV) == 225);
    CHECK(YAW_CABLE_LIMIT_MIN_PULSES == -800L);
    CHECK(YAW_CABLE_LIMIT_MAX_PULSES == 800L);
    CHECK(YAW_STEP_FREQ_MIN_HZ == 20U);
    CHECK(YAW_STEP_FREQ_MAX_HZ == 500U);

    CHECK(Stepper_Init() == STEPPER_STATUS_OK);
    CHECK(YawAxis_Init() == YAW_AXIS_STATUS_OK);
    CHECK(YawAxis_SetCurrentPositionAsZero() == YAW_AXIS_STATUS_OK);
    CHECK(YawAxis_Enable() == YAW_AXIS_STATUS_OK);
    CHECK(YawAxis_ValidateTargetMilliDeg(5000, YAW_STEP_FREQ_MIN_HZ) ==
          YAW_AXIS_STATUS_OK);
    CHECK(YawAxis_ValidateTargetMilliDeg(5000, YAW_STEP_FREQ_MAX_HZ) ==
          YAW_AXIS_STATUS_OK);
    CHECK(YawAxis_ValidateTargetMilliDeg(5000, YAW_STEP_FREQ_MIN_HZ - 1U) ==
          YAW_AXIS_STATUS_INVALID_ARGUMENT);
    CHECK(YawAxis_ValidateTargetMilliDeg(5000, YAW_STEP_FREQ_MAX_HZ + 1U) ==
          YAW_AXIS_STATUS_INVALID_ARGUMENT);
    CHECK(YawAxis_MoveRelativePulses(0, YAW_STEP_FREQ_MIN_HZ) ==
          YAW_AXIS_STATUS_OK);
    CHECK(YawAxis_MoveRelativePulses(0, YAW_STEP_FREQ_MAX_HZ) ==
          YAW_AXIS_STATUS_OK);
    CHECK(YawAxis_MoveRelativePulses(0, YAW_STEP_FREQ_MIN_HZ - 1U) ==
          YAW_AXIS_STATUS_INVALID_ARGUMENT);
    CHECK(YawAxis_MoveRelativePulses(0, YAW_STEP_FREQ_MAX_HZ + 1U) ==
          YAW_AXIS_STATUS_INVALID_ARGUMENT);
}

static void Test_YawDisableDuringRunning(void)
{
    uint32_t now;

    CHECK(Stepper_Init() == STEPPER_STATUS_OK);
    CHECK(YawAxis_Init() == YAW_AXIS_STATUS_OK);
    CHECK(YawAxis_SetCurrentPositionAsZero() == YAW_AXIS_STATUS_OK);
    CHECK(YawAxis_Enable() == YAW_AXIS_STATUS_OK);
    Debug_Init();

    CHECK(YawAxis_MoveRelativePulses(4, 100U) == YAW_AXIS_STATUS_OK);
    now = TestFakes_GetTick();
    TestFakes_SetTick(now + STEPPER_DIRECTION_SETUP_MS);
    YawAxis_Process();
    CHECK(Stepper_GetState() == STEPPER_STATE_RUNNING);
    HAL_TIM_PWM_PulseFinishedCallback(&htim3);
    YawAxis_Process();
    CHECK(Stepper_GetCommandedPosition() == 1);
    CHECK(YawAxis_GetCommandedMilliDeg() == 225);

    CHECK(YawAxis_Disable() == YAW_AXIS_STATUS_OK);
    CHECK(Stepper_GetState() == STEPPER_STATE_STOPPING);
    CHECK(YawAxis_GetReferenceState() == YAW_REFERENCE_MANUAL);
    CHECK(YawAxis_GetCommandedMilliDeg() == 225);
    Debug_Process(100U, 0U);
    CHECK(g_control_debug.state.yaw.stepper_state == STEPPER_STATE_STOPPING);
    CHECK(g_control_debug.state.yaw.reference_state == YAW_REFERENCE_MANUAL);
    CHECK(g_control_debug.state.yaw.cable_margin_valid == 1U);
    CHECK(g_control_debug.state.yaw.commanded_mdeg == 225);

    HAL_TIM_PWM_PulseFinishedCallback(&htim3);
    YawAxis_Process();
    CHECK(Stepper_GetState() == STEPPER_STATE_DISABLED);
    CHECK(YawAxis_GetReferenceState() == YAW_REFERENCE_INVALID);
    CHECK(YawAxis_GetCommandedMilliDeg() == INT32_MIN);
    Debug_Process(120U, 0U);
    CHECK(g_control_debug.state.yaw.cable_margin_valid == 0U);
    CHECK(g_control_debug.state.yaw.commanded_mdeg == INT32_MIN);
    CHECK(g_control_debug.state.yaw.cable_margin_to_min_mdeg == INT32_MIN);
    CHECK(g_control_debug.state.yaw.cable_remaining_negative_pulses == 0U);
    CHECK(g_control_debug.state.yaw.cable_remaining_positive_pulses == 0U);
}

static void Test_YawFaultInvalidatesReference(void)
{
    uint32_t now;

    CHECK(Stepper_Init() == STEPPER_STATUS_OK);
    CHECK(YawAxis_Init() == YAW_AXIS_STATUS_OK);
    CHECK(YawAxis_SetCurrentPositionAsZero() == YAW_AXIS_STATUS_OK);
    CHECK(YawAxis_Enable() == YAW_AXIS_STATUS_OK);
    Debug_Init();
    CHECK(YawAxis_MoveRelativePulses(2, 100U) == YAW_AXIS_STATUS_OK);
    now = TestFakes_GetTick();
    TestFakes_SetTick(now + STEPPER_DIRECTION_SETUP_MS);
    TestFakes_FailNextPwmStart();
    YawAxis_Process();

    CHECK(Stepper_GetState() == STEPPER_STATE_FAULT);
    CHECK(YawAxis_GetReferenceState() == YAW_REFERENCE_INVALID);
    CHECK(YawAxis_GetCommandedMilliDeg() == INT32_MIN);
    Debug_Process(200U, 0U);
    CHECK(g_control_debug.state.yaw.cable_margin_valid == 0U);
    CHECK(g_control_debug.state.yaw.commanded_mdeg == INT32_MIN);
}

static void Test_AxisConversionsAndReference(void)
{
    CHECK(YAW_AXIS_PULSES_PER_REV == 1600U);
    CHECK(Servo_Init() == SERVO_STATUS_OK);
    CHECK(Stepper_Init() == STEPPER_STATUS_OK);
    CHECK(PitchAxis_Init() == PITCH_AXIS_STATUS_OK);
    CHECK(YawAxis_Init() == YAW_AXIS_STATUS_OK);
    CHECK(YawAxis_IsSoftLimitEnabled());
    CHECK(YawAxis_GetSoftLimitMinMilliDeg() == YAW_CABLE_LIMIT_MIN_MDEG);
    CHECK(YawAxis_GetSoftLimitMaxMilliDeg() == YAW_CABLE_LIMIT_MAX_MDEG);
    CHECK(PitchAxis_IsSoftLimitEnabled());
    CHECK(PitchAxis_GetSoftLimitMinMilliDeg() == -30000);
    CHECK(PitchAxis_GetSoftLimitMaxMilliDeg() == 30000);
    CHECK(PitchAxis_GetTargetMilliDeg() == 0);
    CHECK(PitchAxis_GetCommandedMilliDeg() == 0);
    CHECK(PitchAxis_GetServoTargetMilliDeg() == PITCH_LEVEL_SERVO_MDEG);
    CHECK(PitchAxis_GetPulseUs() == SERVO_CENTER_PULSE_US);
    CHECK(YawAxis_GetReferenceState() == YAW_REFERENCE_INVALID);
    CHECK(YawAxis_GetCommandedMilliDeg() == INT32_MIN);
    CHECK(YawAxis_GetMeasuredMilliDeg() == INT32_MIN);
    CHECK(!YawAxis_IsMeasurementValid());
    CHECK(YawAxis_SetTargetMilliDeg(0, 20U) == YAW_AXIS_STATUS_NOT_REFERENCED);
    CHECK(YawAxis_MoveRelativePulses(1, 20U) == YAW_AXIS_STATUS_NOT_REFERENCED);
    CHECK(YawAxis_SetCurrentPositionAsZero() == YAW_AXIS_STATUS_OK);
    CHECK(YawAxis_GetReferenceState() == YAW_REFERENCE_MANUAL);
    CHECK(YawAxis_GetCommandedMilliDeg() == 0);
    CHECK(YawAxis_GetZeroOffsetPulses() == 0);
    CHECK(YawAxis_Enable() == YAW_AXIS_STATUS_OK);
    CHECK(YawAxis_SetCurrentPositionAsZero() == YAW_AXIS_STATUS_INVALID_STATE);

#if (RAW_BENCH_COMMANDS_ENABLE == 1)
    CHECK(PitchAxis_SetRawPulseUs(SERVO_MIN_PULSE_US - 1U) ==
          PITCH_AXIS_STATUS_INVALID_ARGUMENT);
    CHECK(PitchAxis_SetRawPulseUs(SERVO_MAX_PULSE_US + 1U) ==
          PITCH_AXIS_STATUS_INVALID_ARGUMENT);
    CHECK(PitchAxis_SetRawPulseUs(SERVO_MIN_PULSE_US) == PITCH_AXIS_STATUS_LIMIT);
    CHECK(PitchAxis_SetRawPulseUs(SERVO_MAX_PULSE_US) == PITCH_AXIS_STATUS_LIMIT);
    CHECK(PitchAxis_SetRawPulseUs(1510U) == PITCH_AXIS_STATUS_OK);
    CHECK(PitchAxis_IsRawPulseMode());
    CHECK(PitchAxis_IsCommandedAngleValid());
    CHECK(PitchAxis_GetTargetMilliDeg() == INT32_MIN);
    CHECK((PitchAxis_GetCommandedMilliDeg() >= PITCH_SOFT_MIN_MDEG) &&
          (PitchAxis_GetCommandedMilliDeg() <= PITCH_SOFT_MAX_MDEG));
#else
    CHECK(PitchAxis_SetRawPulseUs(1510U) == PITCH_AXIS_STATUS_DISABLED);
    CHECK(!PitchAxis_IsRawPulseMode());
    CHECK(PitchAxis_GetTargetMilliDeg() == 0);
#endif
    CHECK(!PitchAxis_IsMeasurementValid());
    CHECK(PitchAxis_GetMeasuredMilliDeg() == INT32_MIN);

    CHECK(YawAxis_ValidateTargetMilliDeg(-180000, 20U) == YAW_AXIS_STATUS_OK);
    CHECK(YawAxis_ValidateTargetMilliDeg(180000, 20U) == YAW_AXIS_STATUS_OK);
    CHECK(YawAxis_ValidateTargetMilliDeg(-180225, 20U) == YAW_AXIS_STATUS_LIMIT);
    CHECK(YawAxis_ValidateTargetMilliDeg(180225, 20U) == YAW_AXIS_STATUS_LIMIT);
    CHECK(YawAxis_ValidateTargetMilliDeg(-180001, 20U) == YAW_AXIS_STATUS_LIMIT);
    CHECK(YawAxis_ValidateTargetMilliDeg(180001, 20U) == YAW_AXIS_STATUS_LIMIT);
    CHECK(YawAxis_GetLimitRejectCount() == 0U);
    CHECK(YawAxis_SetTargetMilliDeg(180225, 20U) == YAW_AXIS_STATUS_LIMIT);
    CHECK(YawAxis_GetLimitRejectCount() == 1U);

    Test_CompleteYawTarget(225, 1, 225);
    Test_CompleteYawTarget(180000, 800, 180000);
    Test_CompleteYawTarget(-180000, -800, -180000);
    Test_CompleteYawTarget(1000, 4, 900);
    Test_CompleteYawTarget(0, 0, 0);
}

static void Test_PitchLimitsAndTrajectory(void)
{
    uint32_t now = TestFakes_GetTick();
    uint16_t expected_pulse_us;

    CHECK(Servo_Init() == SERVO_STATUS_OK);
    CHECK(PitchAxis_Init() == PITCH_AXIS_STATUS_OK);
    PitchAxis_Process(now);

    CHECK(PitchAxis_GetResponseTimeMs() == PITCH_RESPONSE_TIME_DEFAULT_MS);
    CHECK(PitchAxis_ValidateTargetMilliDeg(PITCH_SOFT_MIN_MDEG) == PITCH_AXIS_STATUS_OK);
    CHECK(PitchAxis_ValidateTargetMilliDeg(PITCH_SOFT_MAX_MDEG) == PITCH_AXIS_STATUS_OK);
    CHECK(PitchAxis_ValidateTargetMilliDeg(PITCH_SOFT_MIN_MDEG - 1) == PITCH_AXIS_STATUS_LIMIT);
    CHECK(PitchAxis_ValidateTargetMilliDeg(PITCH_SOFT_MAX_MDEG + 1) == PITCH_AXIS_STATUS_LIMIT);
    CHECK(PitchAxis_SetTargetMilliDeg(PITCH_SOFT_MAX_MDEG + 1) == PITCH_AXIS_STATUS_LIMIT);
    CHECK(PitchAxis_GetTargetMilliDeg() == 0);

    CHECK(PitchAxis_SetResponseTimeMs(199U) == PITCH_AXIS_STATUS_INVALID_ARGUMENT);
    CHECK(PitchAxis_GetResponseTimeMs() == PITCH_RESPONSE_TIME_DEFAULT_MS);
    CHECK(PitchAxis_SetResponseTimeMs(200U) == PITCH_AXIS_STATUS_OK);
    CHECK(PitchAxis_SetResponseTimeMs(1000U) == PITCH_AXIS_STATUS_OK);
    CHECK(PitchAxis_SetResponseTimeMs(5000U) == PITCH_AXIS_STATUS_OK);
    CHECK(PitchAxis_SetResponseTimeMs(5001U) == PITCH_AXIS_STATUS_INVALID_ARGUMENT);
    CHECK(PitchAxis_GetResponseTimeMs() == 5000U);
    CHECK(PitchAxis_SetResponseTimeMs(PITCH_RESPONSE_TIME_DEFAULT_MS) == PITCH_AXIS_STATUS_OK);

    CHECK(PitchAxis_SetTargetMilliDeg(30000) == PITCH_AXIS_STATUS_OK);
    CHECK(PitchAxis_IsMoving());
    CHECK(PitchAxis_GetCommandedMilliDeg() == 0);
    CHECK(PitchAxis_GetServoTargetMilliDeg() == PITCH_LEVEL_SERVO_MDEG);
    PitchAxis_Process(now + 19U);
    CHECK(PitchAxis_GetCommandedMilliDeg() == 0);
    PitchAxis_Process(now + 500U);
    CHECK(PitchAxis_GetCommandedMilliDeg() == 15000);
    CHECK(PitchAxis_GetTrajectoryElapsedMs() == 500U);
    CHECK(PitchAxis_GetServoTargetMilliDeg() == (PITCH_LEVEL_SERVO_MDEG + 15000));

    CHECK(PitchAxis_SetTargetMilliDeg(-10000) == PITCH_AXIS_STATUS_OK);
    CHECK(PitchAxis_GetCommandedMilliDeg() == 15000);
    CHECK(PitchAxis_GetTargetMilliDeg() == -10000);
    CHECK(PitchAxis_GetActiveResponseTimeMs() == 1000U);
    PitchAxis_Process(now + 520U);
    CHECK(PitchAxis_GetCommandedMilliDeg() == 14500);

    CHECK(PitchAxis_SetResponseTimeMs(2000U) == PITCH_AXIS_STATUS_OK);
    CHECK(PitchAxis_GetResponseTimeMs() == 2000U);
    CHECK(PitchAxis_GetActiveResponseTimeMs() == 1000U);
    PitchAxis_Process(now + 1500U);
    CHECK(PitchAxis_GetCommandedMilliDeg() == -10000);
    CHECK(!PitchAxis_IsMoving());

    CHECK(PitchAxis_SetTargetMilliDeg(20000) == PITCH_AXIS_STATUS_OK);
    CHECK(PitchAxis_GetActiveResponseTimeMs() == 2000U);
    PitchAxis_Process(now + 1519U);
    CHECK(PitchAxis_GetCommandedMilliDeg() == -10000);
    PitchAxis_Process(now + 1520U);
    CHECK(PitchAxis_GetCommandedMilliDeg() == -9700);

    CHECK(PitchAxis_Init() == PITCH_AXIS_STATUS_OK);
    now += 2000U;
    PitchAxis_Process(now);
    CHECK(PitchAxis_SetResponseTimeMs(200U) == PITCH_AXIS_STATUS_OK);
    CHECK(PitchAxis_SetTargetMilliDeg(-30000) == PITCH_AXIS_STATUS_OK);
    PitchAxis_Process(now + 200U);
    CHECK(PitchAxis_GetCommandedMilliDeg() == -30000);
    CHECK(PitchAxis_GetServoTargetMilliDeg() ==
          (PITCH_LEVEL_SERVO_MDEG + (PITCH_SERVO_DIRECTION_SIGN * PITCH_SOFT_MIN_MDEG)));
    CHECK(Servo_ConvertAngleMilliDegToPulseUs(PitchAxis_GetServoTargetMilliDeg(),
                                              &expected_pulse_us) == SERVO_STATUS_OK);
    CHECK(PitchAxis_GetPulseUs() == expected_pulse_us);

    CHECK(PitchAxis_SetTargetMilliDeg(30000) == PITCH_AXIS_STATUS_OK);
    PitchAxis_Process(now + 400U);
    CHECK(PitchAxis_GetCommandedMilliDeg() == 30000);
    CHECK(PitchAxis_GetServoTargetMilliDeg() ==
          (PITCH_LEVEL_SERVO_MDEG + (PITCH_SERVO_DIRECTION_SIGN * PITCH_SOFT_MAX_MDEG)));
    CHECK(Servo_ConvertAngleMilliDegToPulseUs(PitchAxis_GetServoTargetMilliDeg(),
                                              &expected_pulse_us) == SERVO_STATUS_OK);
    CHECK(PitchAxis_GetPulseUs() == expected_pulse_us);

#if (RAW_BENCH_COMMANDS_ENABLE == 1)
    CHECK(PitchAxis_SetRawServoAngleMilliDeg(PITCH_LEVEL_SERVO_MDEG +
                                            (PITCH_SERVO_DIRECTION_SIGN *
                                             (PITCH_SOFT_MIN_MDEG - 1000))) ==
          PITCH_AXIS_STATUS_LIMIT);
    CHECK(PitchAxis_SetRawServoAngleMilliDeg(PITCH_LEVEL_SERVO_MDEG +
                                            (PITCH_SERVO_DIRECTION_SIGN *
                                             (PITCH_SOFT_MIN_MDEG + 1000))) ==
          PITCH_AXIS_STATUS_OK);
    CHECK(PitchAxis_IsRawPulseMode());
    CHECK(PitchAxis_GetTargetMilliDeg() == INT32_MIN);
    CHECK(PitchAxis_GetCommandedMilliDeg() >= PITCH_SOFT_MIN_MDEG);
    CHECK(PitchAxis_SetTargetMilliDeg(0) == PITCH_AXIS_STATUS_OK);
    CHECK(PitchAxis_GetCommandedMilliDeg() >= PITCH_SOFT_MIN_MDEG);
    CHECK(!PitchAxis_IsRawPulseMode());
#else
    CHECK(PitchAxis_SetRawServoAngleMilliDeg(PITCH_LEVEL_SERVO_MDEG +
                                            (PITCH_SERVO_DIRECTION_SIGN *
                                             (PITCH_SOFT_MIN_MDEG + 1000))) ==
          PITCH_AXIS_STATUS_DISABLED);
#endif
}

static void Test_DebugTelemetryAndMailbox(void)
{
    uint32_t heartbeat;
    uint32_t snapshot_seq;
#if (DEBUG_CONTROL_ENABLE == 1)
    uint32_t pulse_index;
    uint32_t now;
#endif
    int32_t initial_yaw_position;

    CHECK(Stepper_Init() == STEPPER_STATUS_OK);
    CHECK(YawAxis_Init() == YAW_AXIS_STATUS_OK);
    CHECK(YawAxis_GetReferenceState() == YAW_REFERENCE_INVALID);
    CHECK(!YawAxis_IsEnabled());
    CHECK(PitchAxis_Init() == PITCH_AXIS_STATUS_OK);
    PitchAxis_Process(100U);
    Debug_Init();
    Debug_Process(UINT32_MAX - 5U, 0xA5U);
    CHECK(g_control_debug.state.version == DEBUG_STATE_VERSION);
    CHECK(g_control_debug.state.snapshot_seq == 2U);
    CHECK((g_control_debug.state.snapshot_seq & 1U) == 0U);
    CHECK(g_control_debug.state.heartbeat == 1U);
    CHECK(g_control_debug.state.app_health_flags == 0xA5U);
    CHECK(g_control_debug.state.pitch.target_mdeg == 0);
    CHECK(g_control_debug.state.pitch.commanded_mdeg == 0);
    CHECK(g_control_debug.state.pitch.servo_target_mdeg == PITCH_LEVEL_SERVO_MDEG);
    CHECK(g_control_debug.state.pitch.servo_pulse_us == SERVO_CENTER_PULSE_US);
    CHECK(g_control_debug.state.pitch.measured_mdeg == INT32_MIN);
    CHECK(g_control_debug.state.pitch.measurement_valid == 0U);
    CHECK(g_control_debug.state.pitch.soft_limit_min_mdeg == PITCH_SOFT_MIN_MDEG);
    CHECK(g_control_debug.state.pitch.soft_limit_max_mdeg == PITCH_SOFT_MAX_MDEG);
    CHECK(g_control_debug.state.pitch.response_time_ms == PITCH_RESPONSE_TIME_DEFAULT_MS);
    CHECK(g_control_debug.state.yaw.target_mdeg == INT32_MIN);
    CHECK(g_control_debug.state.yaw.quantized_target_mdeg == INT32_MIN);
    CHECK(g_control_debug.state.yaw.commanded_mdeg == INT32_MIN);
    CHECK(g_control_debug.state.yaw.measured_mdeg == INT32_MIN);
    CHECK(g_control_debug.state.yaw.measurement_valid == 0U);
    CHECK(g_control_debug.state.yaw.soft_limit_min_mdeg == -180000);
    CHECK(g_control_debug.state.yaw.soft_limit_max_mdeg == 180000);
    CHECK(g_control_debug.state.yaw.soft_limit_enabled == 1U);
    CHECK(g_control_debug.state.yaw.cable_limit_min_mdeg == -180000);
    CHECK(g_control_debug.state.yaw.cable_limit_max_mdeg == 180000);
    CHECK(g_control_debug.state.yaw.cable_limit_min_pulses == -800);
    CHECK(g_control_debug.state.yaw.cable_limit_max_pulses == 800);
    CHECK(g_control_debug.state.yaw.cable_margin_to_min_mdeg == INT32_MIN);
    CHECK(g_control_debug.state.yaw.cable_margin_to_max_mdeg == INT32_MIN);
    CHECK(g_control_debug.state.yaw.cable_remaining_negative_pulses == 0U);
    CHECK(g_control_debug.state.yaw.cable_remaining_positive_pulses == 0U);
    CHECK(g_control_debug.state.yaw.axis_pulses_per_rev == 1600U);
    CHECK(g_control_debug.state.yaw.mdeg_per_pulse == 225);
    CHECK(g_control_debug.state.yaw.axis_scale_verified == 0U);
    CHECK(g_control_debug.state.yaw.frequency_min_hz == YAW_STEP_FREQ_MIN_HZ);
    CHECK(g_control_debug.state.yaw.frequency_max_hz == YAW_STEP_FREQ_MAX_HZ);
    CHECK(g_control_debug.state.yaw.cable_margin_valid == 0U);

    snapshot_seq = g_control_debug.state.snapshot_seq;
    heartbeat = g_control_debug.state.heartbeat;
    Debug_Process(14U, 0xA5U);
    CHECK(g_control_debug.state.snapshot_seq == snapshot_seq + 2U);
    CHECK((g_control_debug.state.snapshot_seq & 1U) == 0U);
    CHECK(g_control_debug.state.heartbeat == heartbeat + 1U);
    snapshot_seq = g_control_debug.state.snapshot_seq;
    heartbeat = g_control_debug.state.heartbeat;
    Debug_Process(33U, 0x5AU);
    CHECK(g_control_debug.state.snapshot_seq == snapshot_seq);
    CHECK(g_control_debug.state.heartbeat == heartbeat);
    Debug_Process(34U, 0x5AU);
    CHECK(g_control_debug.state.snapshot_seq == snapshot_seq + 2U);
    CHECK((g_control_debug.state.snapshot_seq & 1U) == 0U);
    CHECK(g_control_debug.state.heartbeat == heartbeat + 1U);
    CHECK(g_control_debug.state.app_health_flags == 0x5AU);

    g_control_debug.tuning.pitch_response_time_ms = 800U;
    Debug_Process(35U, 0U);
#if (DEBUG_CONTROL_ENABLE == 1)
    CHECK(PitchAxis_GetResponseTimeMs() == 800U);
    g_control_debug.tuning.pitch_response_time_ms = 5001U;
    Debug_Process(36U, 0U);
    CHECK(PitchAxis_GetResponseTimeMs() == 800U);
    CHECK(g_control_debug.tuning.pitch_response_time_ms == 800U);
#else
    CHECK(PitchAxis_GetResponseTimeMs() == PITCH_RESPONSE_TIME_DEFAULT_MS);
    CHECK(g_control_debug.tuning.pitch_response_time_ms == PITCH_RESPONSE_TIME_DEFAULT_MS);
    g_control_debug.tuning.pitch_response_time_ms = 5001U;
    Debug_Process(36U, 0U);
    CHECK(PitchAxis_GetResponseTimeMs() == PITCH_RESPONSE_TIME_DEFAULT_MS);
    CHECK(g_control_debug.tuning.pitch_response_time_ms == PITCH_RESPONSE_TIME_DEFAULT_MS);
#endif

    g_control_debug.command.pitch_target_mdeg = 10000;
    g_control_debug.command.command = DEBUG_CMD_SET_PITCH_MDEG;
    g_control_debug.command.request_seq = 1U;
    Debug_Process(54U, 0U);
    CHECK(g_control_debug.command.applied_seq == 1U);
    CHECK(g_control_debug.command.command == DEBUG_CMD_NONE);
#if (DEBUG_CONTROL_ENABLE == 1)
    CHECK(g_control_debug.command.result == DEBUG_RESULT_OK);
    CHECK(PitchAxis_GetTargetMilliDeg() == 10000);
    CHECK(PitchAxis_GetPulseUs() == SERVO_CENTER_PULSE_US);
#else
    CHECK(g_control_debug.command.result == DEBUG_RESULT_DISABLED);
    CHECK(PitchAxis_GetTargetMilliDeg() == 0);
#endif

    g_control_debug.command.pitch_target_mdeg = 20000;
    Debug_Process(55U, 0U);
#if (DEBUG_CONTROL_ENABLE == 1)
    CHECK(PitchAxis_GetTargetMilliDeg() == 10000);
#else
    CHECK(PitchAxis_GetTargetMilliDeg() == 0);
#endif
    CHECK(g_control_debug.command.applied_seq == 1U);

    g_control_debug.command.command = DEBUG_CMD_SET_PITCH_MDEG;
    g_control_debug.command.pitch_target_mdeg = 30001;
    g_control_debug.command.request_seq = 2U;
    Debug_Process(56U, 0U);
#if (DEBUG_CONTROL_ENABLE == 1)
    CHECK(g_control_debug.command.result == DEBUG_RESULT_LIMIT);
    CHECK(PitchAxis_GetTargetMilliDeg() == 10000);
#else
    CHECK(g_control_debug.command.result == DEBUG_RESULT_DISABLED);
#endif
    CHECK(g_control_debug.command.applied_seq == 2U);

#if (DEBUG_CONTROL_ENABLE == 1)
    g_control_debug.command.command = DEBUG_CMD_SET_PITCH_RESPONSE_MS;
    g_control_debug.command.pitch_response_time_ms = 1200U;
    g_control_debug.command.request_seq = 3U;
    Debug_Process(57U, 0U);
    CHECK(g_control_debug.command.result == DEBUG_RESULT_OK);
    CHECK(PitchAxis_GetResponseTimeMs() == 1200U);
    CHECK(g_control_debug.tuning.pitch_response_time_ms == 1200U);

    g_control_debug.command.command = DEBUG_CMD_SET_PITCH_RESPONSE_MS;
    g_control_debug.command.pitch_response_time_ms = 5001U;
    g_control_debug.command.request_seq = 4U;
    Debug_Process(58U, 0U);
    CHECK(g_control_debug.command.result == DEBUG_RESULT_INVALID_ARGUMENT);
    CHECK(PitchAxis_GetResponseTimeMs() == 1200U);

    g_control_debug.command.command = DEBUG_CMD_SET_PITCH_PULSE_US;
    g_control_debug.command.pitch_pulse_us = SERVO_MIN_PULSE_US;
    g_control_debug.command.request_seq = 5U;
    Debug_Process(59U, 0U);
    CHECK(g_control_debug.command.result == DEBUG_RESULT_LIMIT);
    CHECK(!PitchAxis_IsRawPulseMode());

    g_control_debug.command.command = DEBUG_CMD_SET_PITCH_PULSE_US;
    g_control_debug.command.pitch_pulse_us = 1510U;
    g_control_debug.command.request_seq = 6U;
    Debug_Process(60U, 0U);
    CHECK(g_control_debug.command.result == DEBUG_RESULT_OK);
    CHECK(PitchAxis_IsRawPulseMode());
    CHECK(PitchAxis_GetTargetMilliDeg() == INT32_MIN);
#else
    g_control_debug.command.command = DEBUG_CMD_SET_PITCH_PULSE_US;
    g_control_debug.command.pitch_pulse_us = 1510U;
    g_control_debug.command.request_seq = 3U;
    Debug_Process(57U, 0U);
    CHECK(g_control_debug.command.result == DEBUG_RESULT_DISABLED);
    CHECK(!PitchAxis_IsRawPulseMode());
#endif

    Debug_Process(74U, 0U);
    initial_yaw_position = Stepper_GetCommandedPosition();
#if (DEBUG_CONTROL_ENABLE == 1)
    g_control_debug.command.command = DEBUG_CMD_SET_YAW_MDEG;
    g_control_debug.command.yaw_target_mdeg = 5000;
    g_control_debug.command.yaw_frequency_hz = 20U;
    g_control_debug.command.request_seq = 7U;
    Debug_Process(75U, 0U);
    CHECK(g_control_debug.command.result == DEBUG_RESULT_NOT_REFERENCED);
    CHECK(Stepper_GetCommandedPosition() == initial_yaw_position);

    g_control_debug.command.command = DEBUG_CMD_SET_YAW_ZERO;
    g_control_debug.command.request_seq = 8U;
    Debug_Process(76U, 0U);
    CHECK(g_control_debug.command.result == DEBUG_RESULT_OK);
    CHECK(YawAxis_GetReferenceState() == YAW_REFERENCE_MANUAL);

    g_control_debug.command.command = DEBUG_CMD_YAW_ENABLE;
    g_control_debug.command.request_seq = 9U;
    Debug_Process(77U, 0U);
    CHECK(g_control_debug.command.result == DEBUG_RESULT_OK);
    CHECK(YawAxis_IsEnabled());

    g_control_debug.command.command = DEBUG_CMD_SET_YAW_ZERO;
    g_control_debug.command.request_seq = 10U;
    Debug_Process(78U, 0U);
    CHECK(g_control_debug.command.result == DEBUG_RESULT_INVALID_STATE);
#endif
    g_control_debug.command.command = DEBUG_CMD_SET_YAW_MDEG;
    g_control_debug.command.yaw_target_mdeg = 1000;
    g_control_debug.command.yaw_frequency_hz = 20U;
#if (DEBUG_CONTROL_ENABLE == 1)
    g_control_debug.command.request_seq = 11U;
    Debug_Process(79U, 0U);
#else
    g_control_debug.command.request_seq = 7U;
    Debug_Process(79U, 0U);
#endif
#if (DEBUG_CONTROL_ENABLE == 1)
    CHECK(g_control_debug.command.applied_seq == 11U);
    CHECK(g_control_debug.command.result == DEBUG_RESULT_OK);
    CHECK(Stepper_GetState() == STEPPER_STATE_DIRECTION_SETUP);
    now = TestFakes_GetTick();
    TestFakes_SetTick(now + STEPPER_DIRECTION_SETUP_MS);
    YawAxis_Process();
    for (pulse_index = 0U; pulse_index < 4U; pulse_index++)
    {
        HAL_TIM_PWM_PulseFinishedCallback(&htim3);
    }
    YawAxis_Process();
    CHECK(Stepper_GetCommandedPosition() == (initial_yaw_position + 4));
#else
    CHECK(g_control_debug.command.applied_seq == 7U);
    CHECK(g_control_debug.command.result == DEBUG_RESULT_DISABLED);
    CHECK(Stepper_GetCommandedPosition() == initial_yaw_position);
#endif
    CHECK(YawAxis_Stop() == YAW_AXIS_STATUS_OK);
    Debug_Process(94U, 0U);
#if (DEBUG_CONTROL_ENABLE == 1)
    CHECK(g_control_debug.state.yaw.cable_margin_valid == 1U);
    CHECK(g_control_debug.state.yaw.commanded_position_pulses == (initial_yaw_position + 4));
    CHECK(g_control_debug.state.yaw.commanded_mdeg == 900);
    CHECK(g_control_debug.state.yaw.remaining_pulses == 0U);
    CHECK(g_control_debug.state.yaw.cable_margin_to_min_mdeg == 180900);
    CHECK(g_control_debug.state.yaw.cable_margin_to_max_mdeg == 179100);
    CHECK(g_control_debug.state.yaw.cable_remaining_negative_pulses == 804U);
    CHECK(g_control_debug.state.yaw.cable_remaining_positive_pulses == 796U);
    CHECK(g_control_debug.state.yaw.cable_limit_reject_count == 0U);
    CHECK(g_control_debug.state.yaw.axis_pulses_per_rev == YAW_AXIS_PULSES_PER_REV);
    CHECK(g_control_debug.state.yaw.mdeg_per_pulse == 225);
    CHECK(g_control_debug.state.yaw.axis_scale_verified == YAW_AXIS_SCALE_VERIFIED);
    CHECK(g_control_debug.state.yaw.frequency_min_hz == YAW_STEP_FREQ_MIN_HZ);
    CHECK(g_control_debug.state.yaw.frequency_max_hz == YAW_STEP_FREQ_MAX_HZ);
    CHECK(g_control_debug.state.last_debug_command == DEBUG_CMD_SET_YAW_MDEG);
    CHECK(g_control_debug.state.last_debug_result == DEBUG_RESULT_OK);
    CHECK(g_control_debug.state.pitch.tuning_reject_count == 1U);
#else
    CHECK(g_control_debug.state.yaw.commanded_position_pulses == initial_yaw_position);
    CHECK(g_control_debug.state.last_debug_command == DEBUG_CMD_SET_YAW_MDEG);
    CHECK(g_control_debug.state.last_debug_result == DEBUG_RESULT_DISABLED);
#endif
#if (DEBUG_CONTROL_ENABLE == 1)
    g_control_debug.command.command = DEBUG_CMD_YAW_DISABLE;
    g_control_debug.command.request_seq = 12U;
    Debug_Process(95U, 0U);
    CHECK(g_control_debug.command.result == DEBUG_RESULT_OK);
    CHECK(YawAxis_GetReferenceState() == YAW_REFERENCE_INVALID);
    CHECK(YawAxis_GetCommandedMilliDeg() == INT32_MIN);

    g_control_debug.command.command = DEBUG_CMD_YAW_ENABLE;
    g_control_debug.command.request_seq = 13U;
    Debug_Process(96U, 0U);
    CHECK(g_control_debug.command.result == DEBUG_RESULT_OK);
    g_control_debug.command.command = DEBUG_CMD_SET_YAW_MDEG;
    g_control_debug.command.yaw_target_mdeg = 0;
    g_control_debug.command.request_seq = 14U;
    Debug_Process(97U, 0U);
    CHECK(g_control_debug.command.result == DEBUG_RESULT_NOT_REFERENCED);
    CHECK(Stepper_GetCommandedPosition() == initial_yaw_position + 4);
    Debug_Process(114U, 0U);
    CHECK(g_control_debug.state.yaw.reference_state == YAW_REFERENCE_INVALID);
    CHECK(g_control_debug.state.yaw.enabled == 1U);
    CHECK(g_control_debug.state.yaw.target_mdeg == INT32_MIN);
    CHECK(g_control_debug.state.yaw.quantized_target_mdeg == INT32_MIN);
    CHECK(g_control_debug.state.yaw.commanded_mdeg == INT32_MIN);
    CHECK(g_control_debug.state.yaw.cable_margin_to_min_mdeg == INT32_MIN);
    CHECK(g_control_debug.state.yaw.cable_margin_to_max_mdeg == INT32_MIN);
    CHECK(g_control_debug.state.yaw.cable_margin_valid == 0U);
#endif
    CHECK((g_control_debug.state.snapshot_seq & 1U) == 0U);
}

int main(void)
{
    Test_TimingConversion();
    Test_StepperProfile();
    Test_InitializeModules();
    Test_FractionalServoCenter();
    Test_ServoStateAndProtocol();
    Test_StepperFiniteMoves();
    Test_ProtocolStepperAndBounds();
    Test_YawReferenceLifecycle();
    Test_YawCablePulseBounds();
    Test_YawNoShortestPath();
    Test_YawAxisScaleAndFrequencyPolicy();
    Test_YawDisableDuringRunning();
    Test_YawFaultInvalidatesReference();
    Test_AxisConversionsAndReference();
    Test_PitchLimitsAndTrajectory();
    Test_DebugTelemetryAndMailbox();

    (void)printf("%u checks, %u failures\n", s_checks, s_failures);
    return (s_failures == 0U) ? 0 : 1;
}

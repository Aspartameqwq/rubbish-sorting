#include "hc04.h"
#include "project_config.h"
#include "protocol.h"
#include "servo.h"
#include "stepper.h"
#include "stepper_profile.h"
#include "tb6600.h"
#include "tb6600_timing.h"
#include "test_fakes.h"
#include "tim.h"

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
    Protocol_Init();
    TestFakes_ResetUart();
    TestFakes_SetTick(100U);
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

    TestFakes_FeedUart("PING\r\n");
    Protocol_Process();
    CHECK(strcmp(TestFakes_TxData(), "PONG\r\n") == 0);
    TestFakes_ClearTx();

    TestFakes_FeedUart("SERVO 1\nSERVO?\n");
    Protocol_Process();
    CHECK(strcmp(TestFakes_TxData(), "OK\r\nSERVO 1\r\n") == 0);
    CHECK(Servo_GetAngle() == 1U);
    TestFakes_ClearTx();

    TestFakes_FeedUart("SERVO_US 1510\nSERVO?\n");
    Protocol_Process();
    CHECK(strcmp(TestFakes_TxData(), "OK\r\nSERVO RAW 1510\r\n") == 0);
    TestFakes_ClearTx();

    TestFakes_FeedUart("SERVO 271\nSERVO_US 999999\n");
    Protocol_Process();
    CHECK(strcmp(TestFakes_TxData(), "ERR\r\nERR\r\n") == 0);
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
    TestFakes_ResetUart();
    Protocol_Init();

    TestFakes_FeedUart("STEPPER?\n");
    Protocol_Process();
    CHECK(strcmp(TestFakes_TxData(), "STEPPER DISABLED POS=0 REM=0 FREQ=0\r\n") == 0);
    TestFakes_ClearTx();

    TestFakes_FeedUart("STEPPER ENABLE\nSTEPPER MOVE 3 1000\n");
    Protocol_Process();
    CHECK(strcmp(TestFakes_TxData(), "OK\r\nOK\r\n") == 0);
    CHECK(Stepper_GetState() == STEPPER_STATE_DIRECTION_SETUP);
    TestFakes_SetTick(105U);
    Stepper_Process();
    CHECK(Stepper_GetState() == STEPPER_STATE_RUNNING);
    HAL_TIM_PWM_PulseFinishedCallback(&htim3);
    HAL_TIM_PWM_PulseFinishedCallback(&htim3);
    CHECK(TB6600_IsPulseRunning());
    HAL_TIM_PWM_PulseFinishedCallback(&htim3);
    CHECK(!TB6600_IsPulseRunning());
    Stepper_Process();
    CHECK(Stepper_GetCommandedPosition() == 3);
    CHECK(Stepper_GetState() == STEPPER_STATE_IDLE);
    TestFakes_ClearTx();

    TestFakes_FeedUart("STEPPER?\n");
    Protocol_Process();
    CHECK(strcmp(TestFakes_TxData(), "STEPPER IDLE POS=3 REM=0 FREQ=0\r\n") == 0);
    TestFakes_ClearTx();

    TestFakes_FeedUart("STEPPER MOVE -2147483648 20\n");
    Protocol_Process();
    CHECK(strcmp(TestFakes_TxData(), "OK\r\n") == 0);
    CHECK(Stepper_GetRemainingSteps() == 2147483648U);
    CHECK(Stepper_Stop() == STEPPER_STATUS_OK);
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
    CHECK(strcmp(TestFakes_TxData(), "OK\r\nOK\r\nOK\r\n") == 0);
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

int main(void)
{
    Test_TimingConversion();
    Test_StepperProfile();
    Test_InitializeModules();
    Test_ServoStateAndProtocol();
    Test_StepperFiniteMoves();
    Test_ProtocolStepperAndBounds();

    (void)printf("%u checks, %u failures\n", s_checks, s_failures);
    return (s_failures == 0U) ? 0 : 1;
}

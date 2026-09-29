#include "hc04.h"
#include "pitch_axis.h"
#include "protocol.h"
#include "servo.h"
#include "sort_task.h"
#include "sort_sequence.h"
#include "stepper.h"
#include "tb6600.h"
#include "test_fakes.h"
#include "yaw_axis.h"

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
            (void)printf("FAIL %s:%d: %s\n", __FILE__, __LINE__, #expression); \
            s_failures++;                                                        \
        }                                                                        \
    } while (0)

static void FeedFrame(const char *content)
{
    char frame[48];
    const size_t length = strlen(content);
    const uint8_t crc = Protocol_Crc8Atm((const uint8_t *)content, (uint16_t)length);

    (void)snprintf(frame, sizeof(frame), "$%s*%02X\n", content, (unsigned int)crc);
    TestFakes_FeedUart(frame);
}

int main(void)
{
    CHECK(Servo_Init() == SERVO_STATUS_OK);
    CHECK(TB6600_Init() == TB6600_STATUS_OK);
    CHECK(Stepper_Init() == STEPPER_STATUS_OK);
    CHECK(PitchAxis_Init() == PITCH_AXIS_STATUS_OK);
    CHECK(YawAxis_Init() == YAW_AXIS_STATUS_OK);
    Protocol_Init();
    SortTask_Init(0U);
    SortSequence_Init();
    TestFakes_SetTick(100U);

    CHECK(!SortTask_ConfigIsValid());
    CHECK(SortTask_AcceptAction(1U, 1U) == SORT_ACCEPT_FAULT);
    FeedFrame("S,1,1");
    Protocol_Process();
    CHECK(protocol_valid_frame_count == 1U);
    CHECK(strstr(TestFakes_TxData(), "$N,1,FAULT*") != NULL);
    CHECK(strstr(TestFakes_TxData(), "$R*") == NULL);
    CHECK(sort_task.state == SORT_STATE_IDLE);
    CHECK(!sort_task.action_valid);

    g_sort_sequence.enabled = 1U;
    SortSequence_Process(TestFakes_GetTick());
    CHECK(g_sort_sequence.status == SORT_SEQUENCE_WAIT_READY);
    CHECK(sort_task.state == SORT_STATE_IDLE);
    CHECK(!sort_task.action_valid);

    (void)printf("%u checks, %u failures\n", s_checks, s_failures);
    return (s_failures == 0U) ? 0 : 1;
}

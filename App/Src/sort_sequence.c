#include "sort_sequence.h"

#include "sort_task.h"

/* Change only this array to change the seven-box test order. */
static const uint8_t s_box_sequence[SORT_SEQUENCE_LENGTH] = {
    2U, 1U, 3U, 4U, 2U, 3U, 1U};

volatile SortSequenceDebug g_sort_sequence;

bool SortSequence_IsEnabled(void)
{
    return g_sort_sequence.enabled == 1U;
}

void SortSequence_Init(void)
{
    g_sort_sequence.enabled = 0U;
    g_sort_sequence.interval_ms = SORT_SEQUENCE_DEFAULT_INTERVAL_MS;
    g_sort_sequence.status = SORT_SEQUENCE_DISABLED;
    g_sort_sequence.next_index = 0U;
    g_sort_sequence.completed_count = 0U;
    g_sort_sequence.active_box = 0U;
    g_sort_sequence.last_result = 0U;
    g_sort_sequence.last_completion_tick_ms = 0U;
}

void SortSequence_Process(uint32_t now_ms)
{
    SortAcceptStatus_t accept_status;

    if (!SortSequence_IsEnabled())
    {
        g_sort_sequence.status = SORT_SEQUENCE_DISABLED;
        g_sort_sequence.next_index = 0U;
        g_sort_sequence.completed_count = 0U;
        g_sort_sequence.active_box = 0U;
        return;
    }

    if ((g_sort_sequence.status == SORT_SEQUENCE_COMPLETE) ||
        (g_sort_sequence.status == SORT_SEQUENCE_FAULT))
    {
        return;
    }

    if (g_sort_sequence.status == SORT_SEQUENCE_RUNNING)
    {
        if (!sort_task.action_completed)
        {
            return;
        }
        g_sort_sequence.last_result = sort_task.result;
        g_sort_sequence.last_completion_tick_ms = now_ms;
        g_sort_sequence.active_box = 0U;
        if ((sort_task.result != 0U) || (sort_task.state == SORT_STATE_FAULT))
        {
            g_sort_sequence.status = SORT_SEQUENCE_FAULT;
            return;
        }
        g_sort_sequence.completed_count++;
        g_sort_sequence.next_index++;
        if (g_sort_sequence.next_index == SORT_SEQUENCE_LENGTH)
        {
            g_sort_sequence.status = SORT_SEQUENCE_COMPLETE;
            return;
        }
    }

    if ((sort_task.state == SORT_STATE_FAULT) ||
        (g_sort_sequence.next_index >= SORT_SEQUENCE_LENGTH))
    {
        g_sort_sequence.status = SORT_SEQUENCE_FAULT;
        return;
    }

    if ((g_sort_sequence.interval_ms < SORT_SEQUENCE_MIN_INTERVAL_MS) ||
        (g_sort_sequence.interval_ms > SORT_SEQUENCE_MAX_INTERVAL_MS))
    {
        g_sort_sequence.status = SORT_SEQUENCE_INVALID_INTERVAL;
        return;
    }

    if ((g_sort_sequence.next_index > 0U) &&
        ((uint32_t)(now_ms - g_sort_sequence.last_completion_tick_ms) <
         g_sort_sequence.interval_ms))
    {
        g_sort_sequence.status = SORT_SEQUENCE_WAIT_INTERVAL;
        return;
    }

    if (!SortTask_IsReady())
    {
        g_sort_sequence.status = SORT_SEQUENCE_WAIT_READY;
        return;
    }

    accept_status = SortTask_AcceptLocalAction(
        s_box_sequence[g_sort_sequence.next_index]);
    if (accept_status != SORT_ACCEPT_ACCEPTED)
    {
        g_sort_sequence.status = (accept_status == SORT_ACCEPT_BUSY)
                                     ? SORT_SEQUENCE_WAIT_READY
                                     : SORT_SEQUENCE_FAULT;
        return;
    }

    g_sort_sequence.active_box = s_box_sequence[g_sort_sequence.next_index];
    g_sort_sequence.status = SORT_SEQUENCE_RUNNING;
    SortTask_StartAcceptedAction(now_ms);
}

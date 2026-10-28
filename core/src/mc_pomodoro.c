#include "mc/mc_pomodoro.h"

bool mc_pomodoro_init(mc_pomodoro_t *p, uint32_t work_ms, uint32_t break_ms)
{
    if (work_ms == 0 || break_ms == 0) {
        return false;
    }
    p->work_ms = work_ms;
    p->break_ms = break_ms;
    mc_pomodoro_stop(p);
    return true;
}

void mc_pomodoro_stop(mc_pomodoro_t *p)
{
    p->phase = MC_POMODORO_IDLE;
    p->start_ms = 0;
    p->remaining_ms = 0;
    p->running = false;
    p->completed = 0;
}

void mc_pomodoro_start(mc_pomodoro_t *p, uint32_t now_ms)
{
    if (p->running) {
        return;
    }
    if (p->phase == MC_POMODORO_IDLE) {
        p->phase = MC_POMODORO_WORK;
        p->remaining_ms = p->work_ms;
    }
    p->start_ms = now_ms;
    p->running = true;
}

uint32_t mc_pomodoro_remaining(const mc_pomodoro_t *p, uint32_t now_ms)
{
    if (!p->running) {
        return p->remaining_ms;
    }
    uint32_t elapsed = now_ms - p->start_ms;
    return elapsed >= p->remaining_ms ? 0u : p->remaining_ms - elapsed;
}

void mc_pomodoro_pause(mc_pomodoro_t *p, uint32_t now_ms)
{
    if (p->running) {
        p->remaining_ms = mc_pomodoro_remaining(p, now_ms);
        p->running = false;
    }
}

mc_pomodoro_event_t mc_pomodoro_poll(mc_pomodoro_t *p, uint32_t now_ms)
{
    if (!p->running || now_ms - p->start_ms < p->remaining_ms) {
        return MC_POMODORO_EV_NONE;
    }
    /* 期限 = 起点 + 剩余；以期限为下一段起点，迟到的部分记在下一段里 */
    p->start_ms += p->remaining_ms;
    if (p->phase == MC_POMODORO_WORK) {
        p->phase = MC_POMODORO_BREAK;
        p->remaining_ms = p->break_ms;
        if (p->completed < UINT16_MAX) {
            p->completed++;
        }
        return MC_POMODORO_EV_WORK_DONE;
    }
    p->phase = MC_POMODORO_WORK;
    p->remaining_ms = p->work_ms;
    return MC_POMODORO_EV_BREAK_DONE;
}

#include "mc/mc_timers.h"

void mc_stopwatch_reset(mc_stopwatch_t *sw)
{
    sw->acc_ms = 0;
    sw->start_ms = 0;
    sw->running = false;
}

void mc_stopwatch_start(mc_stopwatch_t *sw, uint32_t now_ms)
{
    if (!sw->running) {
        sw->start_ms = now_ms;
        sw->running = true;
    }
}

void mc_stopwatch_stop(mc_stopwatch_t *sw, uint32_t now_ms)
{
    if (sw->running) {
        sw->acc_ms += now_ms - sw->start_ms; /* 无符号减法，回绕安全 */
        sw->running = false;
    }
}

uint32_t mc_stopwatch_elapsed(const mc_stopwatch_t *sw, uint32_t now_ms)
{
    return sw->acc_ms + (sw->running ? now_ms - sw->start_ms : 0u);
}

void mc_countdown_set(mc_countdown_t *cd, uint32_t duration_ms)
{
    cd->duration_ms = duration_ms;
    mc_countdown_reset(cd);
}

void mc_countdown_reset(mc_countdown_t *cd)
{
    cd->remaining_ms = cd->duration_ms;
    cd->start_ms = 0;
    cd->running = false;
    cd->fired = false;
}

void mc_countdown_start(mc_countdown_t *cd, uint32_t now_ms)
{
    if (!cd->running && cd->remaining_ms > 0) {
        cd->start_ms = now_ms;
        cd->running = true;
    }
}

uint32_t mc_countdown_remaining(const mc_countdown_t *cd, uint32_t now_ms)
{
    if (!cd->running) {
        return cd->remaining_ms;
    }
    uint32_t elapsed = now_ms - cd->start_ms;
    return elapsed >= cd->remaining_ms ? 0u : cd->remaining_ms - elapsed;
}

void mc_countdown_pause(mc_countdown_t *cd, uint32_t now_ms)
{
    if (cd->running) {
        cd->remaining_ms = mc_countdown_remaining(cd, now_ms);
        cd->running = false;
    }
}

void mc_countdown_ack(mc_countdown_t *cd)
{
    cd->fired = false;
}

bool mc_countdown_poll(mc_countdown_t *cd, uint32_t now_ms)
{
    if (cd->running && mc_countdown_remaining(cd, now_ms) == 0) {
        cd->remaining_ms = 0;
        cd->running = false;
        cd->fired = true;
        return true;
    }
    return false;
}

void mc_timer_bank_init(mc_timer_bank_t *bank)
{
    for (int i = 0; i < MC_STOPWATCH_CHANNELS; ++i) {
        mc_stopwatch_reset(&bank->stopwatch[i]);
    }
    for (int i = 0; i < MC_COUNTDOWN_CHANNELS; ++i) {
        mc_countdown_set(&bank->countdown[i], 0);
    }
}

uint8_t mc_timer_bank_poll(mc_timer_bank_t *bank, uint32_t now_ms)
{
    uint8_t mask = 0;
    for (int i = 0; i < MC_COUNTDOWN_CHANNELS; ++i) {
        if (mc_countdown_poll(&bank->countdown[i], now_ms)) {
            mask |= (uint8_t)(1u << i);
        }
    }
    return mask;
}

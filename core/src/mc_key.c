#include "mc/mc_key.h"

void mc_key_init(mc_key_t *k)
{
    k->stable = false;
    k->candidate = false;
    k->long_sent = false;
    k->since_ms = 0;
    k->press_ms = 0;
}

mc_key_event_t mc_key_poll(mc_key_t *k, bool raw_pressed, uint32_t now_ms)
{
    if (raw_pressed != k->candidate) {
        k->candidate = raw_pressed;
        k->since_ms = now_ms;
        return MC_KEY_EV_NONE;
    }
    if (k->candidate != k->stable) {
        if (now_ms - k->since_ms < MC_KEY_DEBOUNCE_MS) {
            return MC_KEY_EV_NONE;
        }
        k->stable = k->candidate;
        if (!k->stable) {
            return MC_KEY_EV_RELEASE;
        }
        /* 长按从确认时刻算，不从第一次抖动算：抖得久的键不会因此提前报长按 */
        k->press_ms = now_ms;
        k->long_sent = false;
        return MC_KEY_EV_PRESS;
    }
    if (k->stable && !k->long_sent && now_ms - k->press_ms >= MC_KEY_LONG_MS) {
        k->long_sent = true;
        return MC_KEY_EV_LONG;
    }
    return MC_KEY_EV_NONE;
}

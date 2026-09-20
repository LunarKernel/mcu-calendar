/* 多路秒表与多路倒计时。对应选题表扩展功能 3。
 *
 * 所有函数都显式收 now_ms（单调毫秒时基，uint32 回绕安全），自己不读时钟：
 *   - 主循环里传 port_millis()；
 *   - 外部触发计时（扩展功能 4）在 EXTI 中断里抓一个时间戳，回到主循环再传进来，
 *     这样计时精度由抓拍时刻决定，不受主循环轮询周期影响。
 * 本模块不可重入：同一通道不要同时在中断与主循环里操作。
 * 限制：单次连续运行超过 2^32 ms（约 49.7 天）会回绕出错。 */
#ifndef MC_TIMERS_H
#define MC_TIMERS_H

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define MC_STOPWATCH_CHANNELS 4
#define MC_COUNTDOWN_CHANNELS 4

typedef struct {
    uint32_t acc_ms;   /* 已停表段的累计 */
    uint32_t start_ms; /* 当前运行段的起点，仅 running 时有效 */
    bool running;
} mc_stopwatch_t;

void mc_stopwatch_reset(mc_stopwatch_t *sw);
void mc_stopwatch_start(mc_stopwatch_t *sw, uint32_t now_ms); /* 已在跑则无动作 */
void mc_stopwatch_stop(mc_stopwatch_t *sw, uint32_t now_ms);  /* 已停则无动作 */
uint32_t mc_stopwatch_elapsed(const mc_stopwatch_t *sw, uint32_t now_ms);

typedef struct {
    uint32_t duration_ms;  /* 设定值，reset 时恢复 */
    uint32_t remaining_ms; /* 当前运行段起点处的剩余量 */
    uint32_t start_ms;
    bool running;
    bool fired; /* 到点后置位，由使用者读后用 ack 清除 */
} mc_countdown_t;

void mc_countdown_set(mc_countdown_t *cd, uint32_t duration_ms); /* 同时停表并清 fired */
void mc_countdown_start(mc_countdown_t *cd, uint32_t now_ms);    /* 剩余为 0 时无动作 */
void mc_countdown_pause(mc_countdown_t *cd, uint32_t now_ms);
void mc_countdown_reset(mc_countdown_t *cd);
void mc_countdown_ack(mc_countdown_t *cd);
uint32_t mc_countdown_remaining(const mc_countdown_t *cd, uint32_t now_ms);
/* 本次调用恰好到点返回 true（每次到点只返回一次） */
bool mc_countdown_poll(mc_countdown_t *cd, uint32_t now_ms);

typedef struct {
    mc_stopwatch_t stopwatch[MC_STOPWATCH_CHANNELS];
    mc_countdown_t countdown[MC_COUNTDOWN_CHANNELS];
} mc_timer_bank_t;

void mc_timer_bank_init(mc_timer_bank_t *bank);
/* 轮询全部倒计时；返回本次到点的通道位图（bit n = 通道 n） */
uint8_t mc_timer_bank_poll(mc_timer_bank_t *bank, uint32_t now_ms);

#ifdef __cplusplus
}
#endif

#endif

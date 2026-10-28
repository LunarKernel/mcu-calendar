/* 番茄钟：工作段与休息段交替循环（默认 25/5 min）。对应选题表基本功能 3 的「番茄闹钟」。
 *
 * 与 mc_timers 一样显式收 now_ms（单调毫秒时基，uint32 回绕安全），自己不读时钟。
 * 段与段之间按期限衔接：下一段的起点取上一段的期限，而不是轮询到的时刻，
 * 所以主循环轮询周期再粗，循环多少轮也不会累积滞后。
 * 轮询迟到超过一整段时，每次 poll 只推进一段、报一个事件，事件不丢，下一次 poll 接着追。
 * 本模块不可重入，只在主循环里调用。 */
#ifndef MC_POMODORO_H
#define MC_POMODORO_H

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define MC_POMODORO_WORK_MS_DEFAULT (25u * 60u * 1000u)
#define MC_POMODORO_BREAK_MS_DEFAULT (5u * 60u * 1000u)

typedef enum {
    MC_POMODORO_IDLE,
    MC_POMODORO_WORK,
    MC_POMODORO_BREAK,
} mc_pomodoro_phase_t;

typedef enum {
    MC_POMODORO_EV_NONE,
    MC_POMODORO_EV_WORK_DONE,  /* 工作段结束，已转入休息段 */
    MC_POMODORO_EV_BREAK_DONE, /* 休息段结束，已转入下一个工作段 */
} mc_pomodoro_event_t;

typedef struct {
    uint32_t work_ms;
    uint32_t break_ms;
    mc_pomodoro_phase_t phase;
    uint32_t start_ms;     /* 当前运行段的起点，仅 running 时有效 */
    uint32_t remaining_ms; /* 当前运行段起点处的剩余量 */
    bool running;
    uint16_t completed; /* 已完成的工作段数 */
} mc_pomodoro_t;

/* 任一时长为 0 返回 false，番茄钟保持原状；成功则回到 IDLE 并清零计数 */
bool mc_pomodoro_init(mc_pomodoro_t *p, uint32_t work_ms, uint32_t break_ms);
/* IDLE 时从工作段开始；暂停时续跑；已在跑则无动作 */
void mc_pomodoro_start(mc_pomodoro_t *p, uint32_t now_ms);
void mc_pomodoro_pause(mc_pomodoro_t *p, uint32_t now_ms);
void mc_pomodoro_stop(mc_pomodoro_t *p); /* 回到 IDLE 并清零计数，时长设定保留 */
uint32_t mc_pomodoro_remaining(const mc_pomodoro_t *p, uint32_t now_ms);
mc_pomodoro_event_t mc_pomodoro_poll(mc_pomodoro_t *p, uint32_t now_ms);

#ifdef __cplusplus
}
#endif

#endif

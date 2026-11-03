// SPDX-License-Identifier: MIT
#pragma once
#include <stdint.h>
#include <stdbool.h>

#define NANO35_POMO_BLINK_MS 20000
#define NANO35_POMO_HOLD_MS 60000
#define NANO35_POMO_DONE_MS (NANO35_POMO_BLINK_MS + NANO35_POMO_HOLD_MS)
enum nano35_pomo_phase { NANO35_POMO_OFF, NANO35_POMO_RUNNING, NANO35_POMO_DONE };
struct nano35_pomo_state {
    enum nano35_pomo_phase phase;
    int64_t deadline, done_at;
};
static inline void nano35_pomo_reset(struct nano35_pomo_state *s) {
    s->phase = NANO35_POMO_OFF; s->deadline = s->done_at = 0;
}
// A valid start always REPLACES the previous timer, including completion alerts.
static inline bool nano35_pomo_command(struct nano35_pomo_state *s, uint32_t minutes, int64_t now) {
    if (!minutes) { nano35_pomo_reset(s); return true; }
    if (minutes != 5 && minutes != 10 && minutes != 25) { return false; }
    s->phase = NANO35_POMO_RUNNING;
    s->deadline = now + minutes * 60000;
    s->done_at = 0;
    return true;
}
// Notification windows use the real deadline, never a delayed callback's time.
static inline bool nano35_pomo_advance(struct nano35_pomo_state *s, int64_t now) {
    enum nano35_pomo_phase before = s->phase;
    if (s->phase == NANO35_POMO_RUNNING && now >= s->deadline) {
        s->phase = NANO35_POMO_DONE; s->done_at = s->deadline;
    }
    if (s->phase == NANO35_POMO_DONE && now >= s->done_at + NANO35_POMO_DONE_MS) {
        // Hide the timer UI, but retain its deadline for the OLED's post-timer hold.
        // Explicit stop still clears everything through nano35_pomo_reset().
        s->phase = NANO35_POMO_OFF;
    }
    return before != s->phase;
}
static inline uint32_t nano35_pomo_seconds(const struct nano35_pomo_state *s, int64_t now) {
    if (s->phase != NANO35_POMO_RUNNING) { return 0; }
    int64_t ms = s->deadline - now;
    return ms > 0 ? (uint32_t)((ms + 999) / 1000) : 0;
}

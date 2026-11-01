// SPDX-License-Identifier: MIT
#pragma once
#include <stdint.h>
#include <stdbool.h>

enum nano35_pomo_phase { NANO35_POMO_OFF, NANO35_POMO_FOCUS, NANO35_POMO_BREAK, NANO35_POMO_DONE };
struct nano35_pomo_config { uint32_t focus_ms, break_ms; uint8_t rounds; };
struct nano35_pomo_state {
    enum nano35_pomo_phase phase;
    uint8_t round, total_rounds;
    bool running;
    int64_t deadline;
    uint32_t paused_ms;
};

static inline void nano35_pomo_reset(struct nano35_pomo_state *s) {
    s->phase = NANO35_POMO_OFF;
    s->round = s->total_rounds = 0; s->running = false; s->deadline = 0; s->paused_ms = 0;
}

// Absolute deadlines avoid drift when the display is blank or work is delayed.
// At most 2*rounds-1 transitions, including a long catch-up after USB suspend.
static inline bool nano35_pomo_advance(struct nano35_pomo_state *s,
                                     const struct nano35_pomo_config *cfg, int64_t now) {
    bool changed = false;
    while (s->running && now >= s->deadline) {
        changed = true;
        if (s->phase == NANO35_POMO_FOCUS) {
            if (s->round >= cfg->rounds) {
                s->phase = NANO35_POMO_DONE; s->running = false; s->paused_ms = 0;
            } else {
                s->phase = NANO35_POMO_BREAK; s->deadline += cfg->break_ms;
            }
        } else if (s->phase == NANO35_POMO_BREAK) {
            s->phase = NANO35_POMO_FOCUS; s->round++; s->deadline += cfg->focus_ms;
        } else { s->running = false; }
    }
    return changed;
}

static inline void nano35_pomo_toggle(struct nano35_pomo_state *s,
                                    const struct nano35_pomo_config *cfg, int64_t now) {
    nano35_pomo_advance(s, cfg, now);
    if (s->phase == NANO35_POMO_OFF || s->phase == NANO35_POMO_DONE) {
        s->phase = NANO35_POMO_FOCUS; s->round = 1; s->running = true;
        s->total_rounds = cfg->rounds;
        s->deadline = now + cfg->focus_ms; s->paused_ms = 0;
    } else if (s->running) {
        s->paused_ms = (uint32_t)(s->deadline - now); s->running = false;
    } else {
        s->deadline = now + s->paused_ms; s->running = true;
    }
}

static inline uint32_t nano35_pomo_seconds(const struct nano35_pomo_state *s, int64_t now) {
    if (s->phase == NANO35_POMO_OFF || s->phase == NANO35_POMO_DONE) { return 0; }
    int64_t ms = s->running ? s->deadline - now : s->paused_ms;
    return ms > 0 ? (uint32_t)((ms + 999) / 1000) : 0;
}

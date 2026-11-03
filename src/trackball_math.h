// SPDX-License-Identifier: MIT
#pragma once
#include <stdint.h>
#include <stdbool.h>

enum nano35_ball_mode { NANO35_MOVE, NANO35_SCROLL, NANO35_SNIPE, NANO35_FAST };
// Q8 gain: slow movement 1x, progressively up to 1.75x. Speeds are raw
// 400-CPI counts/second, estimated over >=16 ms on the USB central.
#define NANO35_ACCEL_ONE 256
#define NANO35_ACCEL_MAX 448
#define NANO35_ACCEL_START_CPS 400
#define NANO35_ACCEL_FULL_CPS 1600
#define NANO35_ACCEL_WINDOW_MS 16
#define NANO35_ACCEL_RESET_MS 100

struct nano35_ball_state {
    enum nano35_ball_mode mode;
    int32_t remainder[2];
    int32_t frame_magnitude;
    int32_t frame_abs[2];
    int32_t accel_remainder[2];
    uint32_t accel_window_at, last_frame_at, frame_time, window_distance;
    uint16_t accel_gain;
    uint16_t speed_percent, scroll_percent, snipe_percent; // Independent 0=100%, range 20..200.
    bool frame_open, accel_valid, accel_skip_frame;
};

static inline bool nano35_ball_is_move(enum nano35_ball_mode mode) {
    return mode == NANO35_MOVE || mode == NANO35_FAST;
}

static inline void nano35_ball_reset_accel(struct nano35_ball_state *s) {
    s->accel_gain = NANO35_ACCEL_ONE;
    s->accel_remainder[0] = s->accel_remainder[1] = 0;
    s->frame_abs[0] = s->frame_abs[1] = 0;
    s->window_distance = 0;
    s->frame_open = s->accel_valid = s->accel_skip_frame = false;
}

static inline void nano35_ball_adjust_speed(struct nano35_ball_state *s, unsigned mode, int step) {
    if (mode > NANO35_SNIPE) { return; }
    uint16_t *target = mode == NANO35_SCROLL ? &s->scroll_percent :
                       mode == NANO35_SNIPE ? &s->snipe_percent : &s->speed_percent;
    int percent = (*target ? *target : 100) + step;
    if (percent < 20) { percent = 20; }
    if (percent > 200) { percent = 200; }
    *target = percent;
    s->remainder[0] = s->remainder[1] = 0;
    nano35_ball_reset_accel(s);
}

static inline void nano35_ball_set_mode(struct nano35_ball_state *s, enum nano35_ball_mode mode) {
    if (s->mode != mode) {
        s->mode = mode;
        s->remainder[0] = s->remainder[1] = s->frame_magnitude = 0;
        nano35_ball_reset_accel(s);
    }
}

static inline void nano35_ball_begin_frame(struct nano35_ball_state *s, uint32_t now) {
    if (s->frame_open) { return; }
    // Unsigned subtraction also handles the 32-bit uptime wrap.
    if (!s->accel_valid || (uint32_t)(now - s->last_frame_at) >= NANO35_ACCEL_RESET_MS) {
        nano35_ball_reset_accel(s);
        s->accel_window_at = now;
        s->accel_valid = true;
        // The first sample has no known interval. Never infer speed from it.
        s->accel_skip_frame = true;
    }
    s->frame_time = now;
    s->frame_open = true;
}

// Input is already oriented by the unchanged sensor driver on the peripheral.
static inline int32_t nano35_ball_axis(struct nano35_ball_state *s, unsigned axis, int32_t value) {
    if (nano35_ball_is_move(s->mode)) {
        int32_t magnitude = value < 0 ? -value : value;
        s->frame_magnitude += magnitude; // Auto Mouse threshold stays in raw counts.
        s->frame_abs[axis] += magnitude;
        int32_t gain = s->accel_gain ? s->accel_gain : NANO35_ACCEL_ONE;
        int percent = s->speed_percent ? s->speed_percent : 100;
        // FASTER uses the same normal speed and acceleration curve, multiplied by 3/2.
        int numerator = s->mode == NANO35_FAST ? 3 : 1;
        int denominator = NANO35_ACCEL_ONE * 100 * (s->mode == NANO35_FAST ? 2 : 1);
        int64_t scaled = (int64_t)value * gain * percent * numerator + s->accel_remainder[axis];
        int32_t output = scaled / denominator;
        s->accel_remainder[axis] = scaled - (int64_t)output * denominator;
        return output;
    }
    // Keyball: move 1600/4 = 400 CPI; snipe 400/4 = 100 CPI.
    // Nano's sensor remains at 400 CPI: use 1/4 for snipe, 1/8 for
    // scrolling to match Keyball's 1600-CPI / 32-count wheel distance.
    int32_t divisor = s->mode == NANO35_SNIPE ? 4 : 8;
    int percent = s->mode == NANO35_SNIPE ? s->snipe_percent : s->scroll_percent;
    if (!percent) { percent = 100; }
    int64_t sum = (int64_t)value * percent + s->remainder[axis];
    int32_t output = sum / (divisor * 100);
    s->remainder[axis] = sum - (int64_t)output * divisor * 100;
    // Keyball INVERT_SCROLL_X=y, INVERT_SCROLL_Y=n maps +X to +HWHEEL
    // and +Y (cursor down) to -WHEEL.
    return s->mode == NANO35_SCROLL && axis == 1 ? -output : output;
}

static inline bool nano35_ball_end_frame(struct nano35_ball_state *s) {
    bool trigger = nano35_ball_is_move(s->mode) && s->frame_magnitude > 5;
    if (nano35_ball_is_move(s->mode) && !s->accel_skip_frame) {
        // Approximate Euclidean distance without floating point. Both axes of
        // the NEXT report share this gain: never stretch X and Y differently.
        uint32_t x = s->frame_abs[0], y = s->frame_abs[1];
        uint32_t distance = x > y ? x + y / 2 : y + x / 2;
        // Saturate well above the curve's maximum to bound 32-bit arithmetic.
        s->window_distance += distance;
        if (s->window_distance > 65535) { s->window_distance = 65535; }
        uint32_t elapsed = s->frame_time - s->accel_window_at;
        if (elapsed >= NANO35_ACCEL_WINDOW_MS) {
            uint32_t speed = s->window_distance * 1000 / elapsed;
            uint16_t gain = NANO35_ACCEL_ONE;
            if (speed >= NANO35_ACCEL_FULL_CPS) { gain = NANO35_ACCEL_MAX; }
            else if (speed > NANO35_ACCEL_START_CPS) {
                gain += (speed - NANO35_ACCEL_START_CPS) *
                        (NANO35_ACCEL_MAX - NANO35_ACCEL_ONE) /
                        (NANO35_ACCEL_FULL_CPS - NANO35_ACCEL_START_CPS);
            }
            s->accel_gain = gain;
            s->window_distance = 0;
            s->accel_window_at = s->frame_time;
        }
    }
    s->last_frame_at = s->frame_time;
    s->frame_open = s->accel_skip_frame = false;
    s->frame_abs[0] = s->frame_abs[1] = 0;
    s->frame_magnitude = 0;
    return trigger;
}

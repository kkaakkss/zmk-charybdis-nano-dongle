// SPDX-License-Identifier: MIT
#pragma once
#include <stdint.h>
#include <stdbool.h>

enum nano35_ball_mode { NANO35_MOVE, NANO35_SCROLL, NANO35_SNIPE };
struct nano35_ball_state {
    enum nano35_ball_mode mode;
    int32_t remainder[2];
    int32_t frame_magnitude;
};

static inline void nano35_ball_set_mode(struct nano35_ball_state *s, enum nano35_ball_mode mode) {
    if (s->mode != mode) {
        s->mode = mode;
        s->remainder[0] = s->remainder[1] = s->frame_magnitude = 0;
    }
}

// Input is already oriented by the unchanged sensor driver on the peripheral.
static inline int32_t nano35_ball_axis(struct nano35_ball_state *s, unsigned axis, int32_t value) {
    if (s->mode == NANO35_MOVE) {
        s->frame_magnitude += value < 0 ? -value : value;
        return value;
    }
    // Keyball: move 1600/4 = 400 CPI; snipe 400/4 = 100 CPI.
    // Nano's sensor remains at 400 CPI: use 1/4 for snipe, 1/8 for
    // scrolling to match Keyball's 1600-CPI / 32-count wheel distance.
    int32_t divisor = s->mode == NANO35_SNIPE ? 4 : 8;
    int32_t sum = value + s->remainder[axis];
    int32_t output = sum / divisor;
    s->remainder[axis] = sum - output * divisor;
    // Keyball INVERT_SCROLL_X=y, INVERT_SCROLL_Y=n maps +X to +HWHEEL
    // and +Y (cursor down) to -WHEEL.
    return s->mode == NANO35_SCROLL && axis == 1 ? -output : output;
}

static inline bool nano35_ball_end_frame(struct nano35_ball_state *s) {
    bool trigger = s->mode == NANO35_MOVE && s->frame_magnitude > 5;
    s->frame_magnitude = 0;
    return trigger;
}

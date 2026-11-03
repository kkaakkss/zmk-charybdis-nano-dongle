// SPDX-License-Identifier: MIT
#define DT_DRV_COMPAT nano35_trackball_modes
#include <zephyr/device.h>
#include <zephyr/kernel.h>
#include <drivers/input_processor.h>
#include <zmk/keymap.h>
#include <zmk/event_manager.h>
#include <zmk/events/layer_state_changed.h>
#include "trackball_math.h"

#define MOUSE_LAYER DT_INST_PROP(0, mouse_layer)
#define SCROLL_LAYER DT_INST_PROP(0, scroll_layer)
#define SNIPE_LAYER DT_INST_PROP(0, snipe_layer)
#define AUTO_TIMEOUT DT_INST_PROP(0, automouse_timeout_ms)

static struct nano35_ball_state ball;
K_MUTEX_DEFINE(ball_lock);
static const struct device *temporary_layer = DEVICE_DT_GET(DT_INST_PHANDLE(0, temporary_layer));

void nano35_trackball_speed_step(int step) {
    k_mutex_lock(&ball_lock, K_FOREVER);
    nano35_ball_adjust_speed(&ball, step);
    k_mutex_unlock(&ball_lock);
}

static enum nano35_ball_mode current_mode(void) {
    uint8_t highest = zmk_keymap_highest_layer_active();
    if (highest == SNIPE_LAYER) { return NANO35_SNIPE; }
    if (highest == SCROLL_LAYER) { return NANO35_SCROLL; }
    return NANO35_MOVE;
}

static int modes_layer_event(const zmk_event_t *eh) {
    k_mutex_lock(&ball_lock, K_FOREVER);
    nano35_ball_set_mode(&ball, current_mode());
    k_mutex_unlock(&ball_lock);
    return ZMK_EV_EVENT_BUBBLE;
}
ZMK_LISTENER(nano35_ball_modes, modes_layer_event);
ZMK_SUBSCRIPTION(nano35_ball_modes, zmk_layer_state_changed);

static int modes_handle_event(const struct device *dev, struct input_event *event,
                              uint32_t param1, uint32_t param2,
                              struct zmk_input_processor_state *state) {
    if (event->type != INPUT_EV_REL ||
        (event->code != INPUT_REL_X && event->code != INPUT_REL_Y)) {
        return ZMK_INPUT_PROC_CONTINUE;
    }
    unsigned axis = event->code == INPUT_REL_Y;
    uint8_t highest = zmk_keymap_highest_layer_active();
    k_mutex_lock(&ball_lock, K_FOREVER);
    nano35_ball_set_mode(&ball, current_mode());
    enum nano35_ball_mode mode = ball.mode;
    nano35_ball_begin_frame(&ball, k_uptime_get_32());
    event->value = nano35_ball_axis(&ball, axis, event->value);
    if (mode == NANO35_SCROLL) {
        event->code = axis ? INPUT_REL_WHEEL : INPUT_REL_HWHEEL;
    }
    bool activate = event->sync && nano35_ball_end_frame(&ball);
    k_mutex_unlock(&ball_lock);
    // Do not let incidental movement mask NAV/NUM/OSL while typing.
    // Call outside ball_lock: the upstream processor queues layer events.
    if (activate && (highest == 0 || highest == MOUSE_LAYER)) {
        return zmk_input_processor_handle_event(temporary_layer, event, MOUSE_LAYER,
                                                AUTO_TIMEOUT, state);
    }
    return ZMK_INPUT_PROC_CONTINUE;
}

static const struct zmk_input_processor_driver_api modes_api = {
    .handle_event = modes_handle_event,
};
DEVICE_DT_INST_DEFINE(0, NULL, NULL, NULL, NULL, POST_KERNEL,
                      CONFIG_KERNEL_INIT_PRIORITY_DEFAULT, &modes_api);

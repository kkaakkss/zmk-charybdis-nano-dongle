// SPDX-License-Identifier: MIT
#define DT_DRV_COMPAT nano35_pomodoro
#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <drivers/behavior.h>
#include <zmk/behavior.h>
#include "pomodoro.h"

BUILD_ASSERT(DT_NUM_INST_STATUS_OKAY(DT_DRV_COMPAT) == 1, "One shared Pomodoro clock is supported");
static struct nano35_pomo_state pomodoro;
K_MUTEX_DEFINE(pomodoro_lock);

// Non-OLED builds retain valid combo bindings without a display dependency.
__attribute__((weak)) void nano35_pomodoro_ui_changed(bool notify) {}
static void pomodoro_tick(struct k_timer *timer);
K_TIMER_DEFINE(pomodoro_timer, pomodoro_tick, NULL);

static void pomodoro_work_cb(struct k_work *work) {
    k_mutex_lock(&pomodoro_lock, K_FOREVER);
    bool changed = nano35_pomo_advance(&pomodoro, k_uptime_get());
    if (pomodoro.phase == NANO35_POMO_OFF) { k_timer_stop(&pomodoro_timer); }
    k_mutex_unlock(&pomodoro_lock);
    nano35_pomodoro_ui_changed(changed);
}
K_WORK_DEFINE(pomodoro_work, pomodoro_work_cb);
static void pomodoro_tick(struct k_timer *timer) { k_work_submit(&pomodoro_work); }

void nano35_pomodoro_snapshot(struct nano35_pomo_state *out) {
    k_mutex_lock(&pomodoro_lock, K_FOREVER);
    *out = pomodoro;
    k_mutex_unlock(&pomodoro_lock);
}

static int on_pomodoro_pressed(struct zmk_behavior_binding *binding,
                               struct zmk_behavior_binding_event event) {
    k_mutex_lock(&pomodoro_lock, K_FOREVER);
    if (!nano35_pomo_command(&pomodoro, binding->param1, k_uptime_get())) {
        k_mutex_unlock(&pomodoro_lock); return -EINVAL;
    }
    if (pomodoro.phase != NANO35_POMO_OFF) { k_timer_start(&pomodoro_timer, K_SECONDS(1), K_SECONDS(1)); }
    else { k_timer_stop(&pomodoro_timer); }
    k_mutex_unlock(&pomodoro_lock);
    nano35_pomodoro_ui_changed(false);
    return ZMK_BEHAVIOR_OPAQUE;
}
static int on_pomodoro_released(struct zmk_behavior_binding *binding,
                                struct zmk_behavior_binding_event event) {
    return ZMK_BEHAVIOR_OPAQUE;
}

#if IS_ENABLED(CONFIG_ZMK_BEHAVIOR_METADATA)
static const struct behavior_parameter_value_metadata actions[] = {
    {.display_name = "Start 5 minutes", .type = BEHAVIOR_PARAMETER_VALUE_TYPE_VALUE, .value = 5},
    {.display_name = "Start 10 minutes", .type = BEHAVIOR_PARAMETER_VALUE_TYPE_VALUE, .value = 10},
    {.display_name = "Start 25 minutes", .type = BEHAVIOR_PARAMETER_VALUE_TYPE_VALUE, .value = 25},
    {.display_name = "Stop and hide", .type = BEHAVIOR_PARAMETER_VALUE_TYPE_VALUE, .value = 0},
};
static const struct behavior_parameter_metadata_set action_set[] = {
    {.param1_values = actions, .param1_values_len = ARRAY_SIZE(actions)},
};
static int get_metadata(const struct device *dev, struct behavior_parameter_metadata *metadata) {
    *metadata = (struct behavior_parameter_metadata){.sets = action_set, .sets_len = ARRAY_SIZE(action_set)};
    return 0;
}
#endif
static const struct behavior_driver_api pomodoro_api = {
    .locality = BEHAVIOR_LOCALITY_CENTRAL,
    .binding_pressed = on_pomodoro_pressed,
    .binding_released = on_pomodoro_released,
#if IS_ENABLED(CONFIG_ZMK_BEHAVIOR_METADATA)
    .get_parameter_metadata = get_metadata,
#endif
};
BEHAVIOR_DT_INST_DEFINE(0, NULL, NULL, NULL, NULL, POST_KERNEL,
                        CONFIG_KERNEL_INIT_PRIORITY_DEFAULT, &pomodoro_api);

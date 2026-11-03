// SPDX-License-Identifier: MIT
#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <drivers/behavior.h>
#include <zmk/behavior.h>

#if IS_ENABLED(CONFIG_ZMK_SPLIT_ROLE_CENTRAL)
void nano35_trackball_speed_step(unsigned mode, int step);
#endif
static int speed_pressed(struct zmk_behavior_binding *binding,
                         struct zmk_behavior_binding_event event) {
    if (binding->param1 > 5) { return -EINVAL; }
#if IS_ENABLED(CONFIG_ZMK_SPLIT_ROLE_CENTRAL)
    nano35_trackball_speed_step(binding->param1 / 2, binding->param1 % 2 ? 10 : -10);
#endif
    return ZMK_BEHAVIOR_OPAQUE;
}
static int speed_released(struct zmk_behavior_binding *binding,
                          struct zmk_behavior_binding_event event) {
    return ZMK_BEHAVIOR_OPAQUE;
}
static const struct behavior_driver_api speed_api = {
    .locality = BEHAVIOR_LOCALITY_CENTRAL,
    .binding_pressed = speed_pressed,
    .binding_released = speed_released,
};
BEHAVIOR_DT_DEFINE(DT_NODELABEL(ball_speed), NULL, NULL, NULL, NULL, POST_KERNEL,
                   CONFIG_KERNEL_INIT_PRIORITY_DEFAULT, &speed_api);

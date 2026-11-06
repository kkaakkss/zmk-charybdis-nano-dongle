/*
 * Copyright (c) 2024 The ZMK Contributors
 *
 * SPDX-License-Identifier: MIT
 */

// Include LVGL before ZMK keycode macros: key C would expand the C:/ LV_CONF_PATH.
#include <lvgl.h>
#include <zephyr/kernel.h>
#include <zephyr/bluetooth/services/bas.h>

#include <zephyr/logging/log.h>
LOG_MODULE_DECLARE(zmk, CONFIG_ZMK_LOG_LEVEL);

#include <zmk/display.h>
#include <zmk/hid_indicators.h>
#include <zmk/event_manager.h>
#include <zmk/events/hid_indicators_changed.h>

#include "hid_indicators.h"

#define LED_NLCK 0x01
#define LED_CLCK 0x02
#define LED_SLCK 0x04

struct hid_indicators_state {    
    uint8_t hid_indicators;
};

static sys_slist_t widgets = SYS_SLIST_STATIC_INIT(&widgets);

static void lock_text(char text[4], uint8_t indicators) {
    text[0] = (indicators & LED_NLCK) ? 'N' : ' ';
    text[1] = (indicators & LED_CLCK) ? 'C' : ' ';
    text[2] = (indicators & LED_SLCK) ? 'S' : ' ';
    text[3] = '\0';
}
static void set_hid_indicators(lv_obj_t *label, struct hid_indicators_state state) {
    char text[4];
    lock_text(text, state.hid_indicators);
    lv_label_set_text(label, text);
}

void hid_indicators_update_cb(struct hid_indicators_state state) {
    struct zmk_widget_hid_indicators *widget;
    SYS_SLIST_FOR_EACH_CONTAINER(&widgets, widget, node) { set_hid_indicators(widget->obj, state); }
}

static struct hid_indicators_state hid_indicators_get_state(const zmk_event_t *eh) {
    struct zmk_hid_indicators_changed *ev = eh ? as_zmk_hid_indicators_changed(eh) : NULL;
    return (struct hid_indicators_state) {
        .hid_indicators = ev != NULL ? ev->indicators : zmk_hid_indicators_get_current_profile(),
    };
}

ZMK_DISPLAY_WIDGET_LISTENER(widget_hid_indicators, struct hid_indicators_state,
                            hid_indicators_update_cb, hid_indicators_get_state)

ZMK_SUBSCRIPTION(widget_hid_indicators, zmk_hid_indicators_changed);

int zmk_widget_hid_indicators_init(struct zmk_widget_hid_indicators *widget, lv_obj_t *parent) {
    widget->obj = lv_label_create(parent);
    // Three fixed monospace slots, including inactive spaces: N C S.
    lv_obj_set_width(widget->obj, 26);
    lv_label_set_long_mode(widget->obj, LV_LABEL_LONG_CLIP);
    lv_obj_set_style_text_align(widget->obj, LV_TEXT_ALIGN_LEFT, 0);

    sys_slist_append(&widgets, &widget->node);

    widget_hid_indicators_init();

    return 0;
}

lv_obj_t *zmk_widget_hid_indicators_obj(struct zmk_widget_hid_indicators *widget) {
    return widget->obj;
}

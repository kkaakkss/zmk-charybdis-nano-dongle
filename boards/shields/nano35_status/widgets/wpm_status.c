// Load LVGL before ZMK keycode macros on Windows.
#include <lvgl.h>
#include <stdio.h>
#include <string.h>
#include <zephyr/kernel.h>

#include <zephyr/logging/log.h>
LOG_MODULE_DECLARE(zmk, CONFIG_ZMK_LOG_LEVEL);

#include <zmk/display.h>
#include <zmk/event_manager.h>
#include <zmk/events/wpm_state_changed.h>
#include <zmk/display/widgets/layer_status.h>
#include <zmk/endpoints.h>
#include <zmk/keymap.h>
#include <zmk/wpm.h>

#include "wpm_status.h"

static sys_slist_t widgets = SYS_SLIST_STATIC_INIT(&widgets);
struct wpm_status_state
{
    int wpm;
    const char *layer;
};

static struct wpm_status_state get_state(const zmk_event_t *_eh)
{
    const struct zmk_wpm_state_changed *ev = as_zmk_wpm_state_changed(_eh);
    uint8_t index = zmk_keymap_highest_layer_active();

    return (struct wpm_status_state){
        .wpm = ev ? ev->state : zmk_wpm_get_state(),
        .layer = zmk_keymap_layer_name(index)
    };
}

// Pinned ZMK counts HID key releases and defines one word as five characters.
// This is estimated CPM, not OS-aware Korean composition/test scoring.
static uint16_t cpm_from_wpm(int wpm) {
    if (wpm < 0) { return 0; }
    if (wpm > 255) { wpm = 255; }
    return (uint16_t)wpm * 5;
}

static void set_wpm(struct zmk_widget_wpm_status *widget, struct wpm_status_state state)
{
    if(state.layer && state.layer[0] && CONFIG_ZMK_DONGLE_DISPLAY_WPM_DISABLED_LAYERS[0] &&
       strstr(CONFIG_ZMK_DONGLE_DISPLAY_WPM_DISABLED_LAYERS, state.layer) != NULL) {
        lv_label_set_text(widget->wpm_label, "-");
        return;
    }

    char wpm_text[12];
    snprintf(wpm_text, sizeof(wpm_text), "%u", (unsigned)cpm_from_wpm(state.wpm));
    lv_label_set_text(widget->wpm_label, wpm_text);
}

static void wpm_status_update_cb(struct wpm_status_state state)
{
    struct zmk_widget_wpm_status *widget;
    SYS_SLIST_FOR_EACH_CONTAINER(&widgets, widget, node)
    {
        set_wpm(widget, state);
    }
}

ZMK_DISPLAY_WIDGET_LISTENER(widget_wpm_status, struct wpm_status_state,
                            wpm_status_update_cb, get_state)
ZMK_SUBSCRIPTION(widget_wpm_status, zmk_wpm_state_changed);

int zmk_widget_wpm_status_init(struct zmk_widget_wpm_status *widget, lv_obj_t *parent)
{
    widget->obj = lv_obj_create(parent);
    lv_obj_set_size(widget->obj, NANO35_CPM_WIDTH, NANO35_CPM_HEIGHT);
    lv_obj_clear_flag(widget->obj, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_t *unit_label = lv_label_create(widget->obj);
    lv_obj_set_width(unit_label, NANO35_CPM_WIDTH);
    lv_obj_set_style_text_align(unit_label, LV_TEXT_ALIGN_RIGHT, 0);
    lv_obj_set_pos(unit_label, 0, 0);
    lv_label_set_text(unit_label, "CPM");

    widget->wpm_label = lv_label_create(widget->obj);
    lv_obj_set_width(widget->wpm_label, NANO35_CPM_WIDTH);
    lv_obj_set_style_text_align(widget->wpm_label, LV_TEXT_ALIGN_RIGHT, 0);
    lv_obj_set_pos(widget->wpm_label, 0, 9);

    sys_slist_append(&widgets, &widget->node);

    widget_wpm_status_init();
    return 0;
}

lv_obj_t *zmk_widget_wpm_status_obj(struct zmk_widget_wpm_status *widget)
{
    return widget->obj;
}

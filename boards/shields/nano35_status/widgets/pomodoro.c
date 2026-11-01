// SPDX-License-Identifier: MIT
// All LVGL and display driver calls in this file run on the display work queue.
#include <lvgl.h>
#include <stdio.h>
#include <string.h>
#include <zephyr/kernel.h>
#include <zephyr/sys/atomic.h>
#include <zmk/display.h>
#include <zmk/activity.h>
#include <zmk/event_manager.h>
#include <zmk/events/activity_state_changed.h>
#include "pomodoro_logic.h"
// Avoid the widget header's identical basename when including the engine API.
#include "../../../../src/pomodoro.h"
#include "pomodoro.h"

// Exported callbacks in the pinned ZMK display/main.c (not its public API).
// Keep this tiny adapter local; changing ZMK requires rechecking these symbols.
extern void unblank_display_cb(struct k_work *work);
extern void blank_display_cb(struct k_work *work);
BUILD_ASSERT(IS_ENABLED(CONFIG_ZMK_DISPLAY_BLANK_ON_IDLE), "Pomodoro wake requires idle blanking");

static lv_obj_t *phase_label, *time_label, *dots[4];
static atomic_t notify_pending;
static atomic_t alert_active;
static int64_t alert_until;
static void pomodoro_alert_cb(struct k_work *work);
K_WORK_DELAYABLE_DEFINE(pomodoro_alert_work, pomodoro_alert_cb);

static void pomodoro_alert_cb(struct k_work *work) {
    if (k_uptime_get() < alert_until) {
        unblank_display_cb(work);
        k_work_reschedule_for_queue(zmk_display_work_q(), &pomodoro_alert_work, K_SECONDS(1));
    } else {
        atomic_set(&alert_active, 0);
        if (zmk_activity_get_state() != ZMK_ACTIVITY_ACTIVE) { blank_display_cb(work); }
    }
}

static void set_text(lv_obj_t *label, const char *value) {
    if (strcmp(lv_label_get_text(label), value)) { lv_label_set_text(label, value); }
}

static void pomodoro_ui_cb(struct k_work *work) {
    if (!phase_label) { return; }
    struct nano35_pomo_state state;
    nano35_pomodoro_snapshot(&state);
    bool hidden = state.phase == NANO35_POMO_OFF;
    lv_obj_t *items[] = {phase_label, time_label, dots[0], dots[1], dots[2], dots[3]};
    for (unsigned i = 0; i < ARRAY_SIZE(items); i++) {
        if (hidden) { lv_obj_add_flag(items[i], LV_OBJ_FLAG_HIDDEN); }
        else { lv_obj_clear_flag(items[i], LV_OBJ_FLAG_HIDDEN); }
    }
    if (hidden) {
        atomic_set(&notify_pending, 0);
        alert_until = 0; // A reset also cancels an outstanding completion alert.
        atomic_set(&alert_active, 0);
        k_work_cancel_delayable(&pomodoro_alert_work);
        return;
    }
    const char *phase = state.phase == NANO35_POMO_DONE ? "DONE" :
                        !state.running ? "PAUSE" :
                        state.phase == NANO35_POMO_BREAK ? "BREAK" : "FOCUS";
    uint32_t remaining = nano35_pomo_seconds(&state, k_uptime_get());
    char time[6];
    snprintf(time, sizeof(time), "%02u:%02u", (unsigned)(remaining / 60), (unsigned)(remaining % 60));
    set_text(phase_label, phase); set_text(time_label, time);
    for (unsigned i = 0; i < ARRAY_SIZE(dots); i++) {
        if (i >= state.total_rounds) { lv_obj_add_flag(dots[i], LV_OBJ_FLAG_HIDDEN); continue; }
        // A 1-bit OLED cannot show the mockup's dim dots: small dots mark future rounds.
        bool filled = i < state.round;
        lv_obj_set_size(dots[i], filled ? 3 : 1, filled ? 3 : 1);
        lv_obj_set_pos(dots[i], 50 + i * 6 + (filled ? 0 : 1), filled ? 30 : 31);
    }
    if (atomic_set(&notify_pending, 0)) {
        alert_until = k_uptime_get() + 15000;
        atomic_set(&alert_active, 1);
        pomodoro_alert_cb(work);
    }
}
K_WORK_DEFINE(pomodoro_ui_work, pomodoro_ui_cb);

void nano35_pomodoro_ui_changed(bool notify) {
    if (notify) { atomic_set(&notify_pending, 1); }
    if (zmk_display_is_initialized()) {
        k_work_submit_to_queue(zmk_display_work_q(), &pomodoro_ui_work);
    }
}

// If native idle blanking coincides with a phase alert, restore the alert AFTER
// the native queued blank callback. No fabricated key/activity events are sent.
static int pomodoro_activity_cb(const zmk_event_t *eh) {
    const struct zmk_activity_state_changed *event = as_zmk_activity_state_changed(eh);
    if (event && event->state != ZMK_ACTIVITY_ACTIVE && atomic_get(&alert_active)) {
        k_work_reschedule_for_queue(zmk_display_work_q(), &pomodoro_alert_work, K_MSEC(1));
    }
    return ZMK_EV_EVENT_BUBBLE;
}
ZMK_LISTENER(nano35_pomodoro_activity, pomodoro_activity_cb);
ZMK_SUBSCRIPTION(nano35_pomodoro_activity, zmk_activity_state_changed);

void nano35_widget_pomodoro_init(lv_obj_t *parent) {
    // Existing modifier container is 17 px high, so HID locks begin at y=36.
    // Two 9 px UNSCII lines fit exactly between the top icons and that row.
    phase_label = lv_label_create(parent); lv_obj_set_pos(phase_label, 0, 18);
    time_label = lv_label_create(parent); lv_obj_set_pos(time_label, 0, 27);
    for (unsigned i = 0; i < ARRAY_SIZE(dots); i++) {
        dots[i] = lv_obj_create(parent);
        lv_obj_remove_style_all(dots[i]);
        lv_obj_set_style_bg_color(dots[i], lv_obj_get_style_text_color(parent, LV_PART_MAIN), 0);
        lv_obj_set_style_bg_opa(dots[i], LV_OPA_COVER, 0);
    }
    pomodoro_ui_cb(NULL);
}

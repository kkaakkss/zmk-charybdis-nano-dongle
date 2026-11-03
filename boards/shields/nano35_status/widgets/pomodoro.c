// SPDX-License-Identifier: MIT
// All LVGL and display driver calls run on the dedicated display work queue.
#include <lvgl.h>
#include <stdio.h>
#include <string.h>
#include <zephyr/kernel.h>
#include <zephyr/sys/atomic.h>
#include <zmk/display.h>
#include <zmk/activity.h>
#include <zmk/event_manager.h>
#include <zmk/events/activity_state_changed.h>
#include "../../../../src/pomodoro.h"
#include "pomodoro.h"

// Exported callbacks in the pinned ZMK display/main.c. Recheck on ZMK upgrades.
extern void unblank_display_cb(struct k_work *work);
extern void blank_display_cb(struct k_work *work);
BUILD_ASSERT(IS_ENABLED(CONFIG_ZMK_DISPLAY_BLANK_ON_IDLE), "Timer alerts require idle blanking");

static lv_obj_t *phase_label, *time_label, *party_icon, *confetti_icon;
static atomic_t alert_active;
// Original 8x8 celebration glyphs: transparent background, foreground palette.
static uint8_t party_map[] = {0,0,0,0, 0,0,0,255, 0x10,0x38,0x7c,0xe6,0x54,0x44,0x38,0x00};
static uint8_t confetti_map[] = {0,0,0,0, 0,0,0,255, 0x82,0x25,0x08,0x50,0x12,0x04,0x28,0x41};
static const lv_img_dsc_t party_image = {
    .header = {.cf=LV_IMG_CF_INDEXED_1BIT, .w=8, .h=8}, .data_size=sizeof(party_map), .data=party_map,
};
static const lv_img_dsc_t confetti_image = {
    .header = {.cf=LV_IMG_CF_INDEXED_1BIT, .w=8, .h=8}, .data_size=sizeof(confetti_map), .data=confetti_map,
};
static void pomodoro_ui_cb(struct k_work *work);
static void pomodoro_alert_cb(struct k_work *work) { pomodoro_ui_cb(work); }
K_WORK_DELAYABLE_DEFINE(pomodoro_alert_work, pomodoro_alert_cb);

static void restore_idle_policy(struct k_work *work) {
    k_work_cancel_delayable(&pomodoro_alert_work);
    if (atomic_set(&alert_active, 0)) {
        if (zmk_activity_get_state() == ZMK_ACTIVITY_ACTIVE) { unblank_display_cb(work); }
        else { blank_display_cb(work); }
    }
}
static void set_text(lv_obj_t *label, const char *value) {
    if (strcmp(lv_label_get_text(label), value)) { lv_label_set_text(label, value); }
}
static void pomodoro_ui_cb(struct k_work *work) {
    if (!phase_label) { return; }
    struct nano35_pomo_state state;
    nano35_pomodoro_snapshot(&state);
    int64_t now = k_uptime_get();
    // Derive the view at this timestamp even if the model's 1-second work is late.
    nano35_pomo_advance(&state, now);
    bool hidden = state.phase == NANO35_POMO_OFF;
    bool done = state.phase == NANO35_POMO_DONE;
    lv_obj_t *items[] = {phase_label, time_label, party_icon, confetti_icon};
    for (unsigned i=0; i<ARRAY_SIZE(items); i++) {
        if (hidden || (i>=2 && !done)) { lv_obj_add_flag(items[i], LV_OBJ_FLAG_HIDDEN); }
        else { lv_obj_clear_flag(items[i], LV_OBJ_FLAG_HIDDEN); }
    }
    // Natural completion keeps the screen awake until five minutes after the
    // absolute deadline, even after DONE disappears. Explicit stop clears it.
    int64_t wake_until = state.deadline ? state.deadline + CONFIG_ZMK_IDLE_TIMEOUT : 0;
    if (hidden) {
        if (now < wake_until) {
            atomic_set(&alert_active, 1);
            unblank_display_cb(work);
            k_work_reschedule_for_queue(zmk_display_work_q(), &pomodoro_alert_work,
                                        K_MSEC((int32_t)(wake_until - now)));
        } else { restore_idle_policy(work); }
        return;
    }
    uint32_t remaining = nano35_pomo_seconds(&state, now);
    char time[6];
    snprintf(time, sizeof(time), "%02u:%02u", (unsigned)(remaining/60), (unsigned)(remaining%60));
    set_text(phase_label, done ? "DONE" : "TIMER"); set_text(time_label, time);
    if (!done) {
        atomic_set(&alert_active, 1);
        k_work_cancel_delayable(&pomodoro_alert_work);
        unblank_display_cb(work);
        return;
    }

    atomic_set(&alert_active, 1);
    int64_t elapsed = now - state.done_at;
    // Whole-panel on/off, one full blink per second, for exactly ten seconds.
    bool blink_off = elapsed < NANO35_POMO_BLINK_MS && (elapsed/500)%2;
    if (blink_off) { blank_display_cb(work); }
    else { unblank_display_cb(work); }
    int32_t delay = elapsed < NANO35_POMO_BLINK_MS ? 500-(int32_t)(elapsed%500) : 1000;
    int64_t until_hide = state.done_at + NANO35_POMO_DONE_MS - now;
    if (until_hide < delay) { delay = (int32_t)until_hide; }
    k_work_reschedule_for_queue(zmk_display_work_q(), &pomodoro_alert_work, K_MSEC(delay));
}
K_WORK_DEFINE(pomodoro_ui_work, pomodoro_ui_cb);

void nano35_pomodoro_ui_changed(bool notify) {
    (void)notify;
    if (zmk_display_is_initialized()) {
        k_work_submit_to_queue(zmk_display_work_q(), &pomodoro_ui_work);
    }
}
// Apply the timer screen policy AFTER native blank/unblank work, including a key press while
// the screen is in an off half-cycle. Key input itself remains untouched.
static int pomodoro_activity_cb(const zmk_event_t *eh) {
    const struct zmk_activity_state_changed *event = as_zmk_activity_state_changed(eh);
    if (event) {
        // The first 5-minute expiry can coincide with native 5-minute blanking,
        // BEFORE the UI sets alert_active or the model's periodic work runs.
        struct nano35_pomo_state state;
        nano35_pomodoro_snapshot(&state);
        nano35_pomo_advance(&state, k_uptime_get());
        int64_t wake_until = state.deadline ? state.deadline + CONFIG_ZMK_IDLE_TIMEOUT : 0;
        if (state.phase == NANO35_POMO_RUNNING || k_uptime_get() < wake_until ||
            atomic_get(&alert_active)) {
            k_work_reschedule_for_queue(zmk_display_work_q(), &pomodoro_alert_work, K_MSEC(1));
        }
    }
    return ZMK_EV_EVENT_BUBBLE;
}
ZMK_LISTENER(nano35_pomodoro_activity, pomodoro_activity_cb);
ZMK_SUBSCRIPTION(nano35_pomodoro_activity, zmk_activity_state_changed);

void nano35_widget_pomodoro_init(lv_obj_t *parent) {
    phase_label=lv_label_create(parent); lv_obj_set_pos(phase_label,0,18);
    time_label=lv_label_create(parent); lv_obj_set_pos(time_label,0,27);
    uint32_t ink=lv_color_to32(lv_obj_get_style_text_color(parent,LV_PART_MAIN));
    memcpy(party_map+4,&ink,4); memcpy(confetti_map+4,&ink,4);
    party_icon=lv_img_create(parent); lv_img_set_src(party_icon,&party_image); lv_obj_set_pos(party_icon,38,19);
    confetti_icon=lv_img_create(parent); lv_img_set_src(confetti_icon,&confetti_image); lv_obj_set_pos(confetti_icon,48,19);
    pomodoro_ui_cb(NULL);
}

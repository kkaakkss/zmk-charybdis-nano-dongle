// SPDX-License-Identifier: MIT
// Included after the actual model, behavior and widget C by test-pomodoro.py.
#define CHECK(condition, code) do { if (!(condition)) return code; } while (0)
static void drain(void) {
    for (unsigned guard = 0; queue_count && guard < 32; guard++) {
        struct k_work *work = queued[0];
        for (unsigned i = 1; i < queue_count; i++) queued[i-1] = queued[i];
        queue_count--; work->pending = false;
        current_queue = work->queue == &display_queue ? 1 : 0;
        work->handler(work); current_queue = 0;
    }
    if (queue_count) failure = 90;
}
static void press(unsigned action) {
    struct zmk_behavior_binding binding = {.param1 = action};
    on_pomodoro_pressed(&binding, (struct zmk_behavior_binding_event){0}); drain();
}
static void poll_at(int64_t time) {
    now = time; pomodoro_tick(&pomodoro_timer); drain();
}
static void alert_at(int64_t time) {
    now = time; current_queue = 1; pomodoro_alert_cb(&pomodoro_alert_work.work); current_queue = 0;
}
static bool inside_layout(void) {
    for (unsigned i = 1; i < object_count; i++) {
        lv_obj_t *o = &objects[i];
        if (!o->hidden && (o->x < 0 || o->y < 18 || o->x + o->w > 74 || o->y + o->h > 36)) return false;
    }
    return true;
}
int main(void) {
    // Same model at 1-second and heavily delayed polling must finish identically.
    const struct nano35_pomo_config c = {1500000, 300000, 4};
    struct nano35_pomo_state s = {0};
    nano35_pomo_toggle(&s, &c, 0);
    CHECK(nano35_pomo_seconds(&s, 0) == 1500, 1);
    CHECK(nano35_pomo_seconds(&s, 1) == 1500 && nano35_pomo_seconds(&s, 1000) == 1499, 2);
    CHECK(!nano35_pomo_advance(&s, &c, 1499999), 3);
    CHECK(nano35_pomo_advance(&s, &c, 1500000) && s.phase == NANO35_POMO_BREAK, 4);
    CHECK(s.round == 1 && nano35_pomo_seconds(&s, 1500000) == 300, 5);
    CHECK(nano35_pomo_advance(&s, &c, 1800000) && s.round == 2 && s.phase == NANO35_POMO_FOCUS, 6);
    nano35_pomo_reset(&s); nano35_pomo_toggle(&s, &c, 0);
    unsigned changes = 0;
    for (int64_t t = 1000; t <= 6900000; t += 1000) changes += nano35_pomo_advance(&s, &c, t);
    CHECK(changes == 7 && s.phase == NANO35_POMO_DONE && s.round == 4 && !s.running, 7);
    nano35_pomo_reset(&s); nano35_pomo_toggle(&s, &c, 0);
    nano35_pomo_advance(&s, &c, 9000000);
    CHECK(s.phase == NANO35_POMO_DONE && !s.running && nano35_pomo_seconds(&s, 9000000) == 0, 8);
    nano35_pomo_toggle(&s, &c, 9000000);
    CHECK(s.round == 1 && s.phase == NANO35_POMO_FOCUS, 9);
    // Pause at millisecond precision; no drift or elapsed time during a pause.
    nano35_pomo_toggle(&s, &c, 9000001);
    CHECK(!s.running && s.paused_ms == 1499999, 10);
    CHECK(!nano35_pomo_advance(&s, &c, 12000000), 11);
    nano35_pomo_toggle(&s, &c, 12000000);
    CHECK(s.deadline == 13499999 && s.running, 12);
    nano35_pomo_toggle(&s, &c, 13499999); // Boundary belongs to the new break.
    CHECK(s.phase == NANO35_POMO_BREAK && !s.running && s.paused_ms == 300000, 13);
    nano35_pomo_reset(&s); nano35_pomo_toggle(&s, &c, INT64_C(5000000000));
    nano35_pomo_advance(&s, &c, INT64_C(5001500000));
    CHECK(s.phase == NANO35_POMO_BREAK && nano35_pomo_seconds(&s, INT64_C(5001500000)) == 300, 14);
    // Actual behavior and UI callbacks: queue-only LVGL, hidden initial/reset screen.
    current_queue = 1; nano35_widget_pomodoro_init(&objects[object_count++]); current_queue = 0;
    CHECK(phase_label->hidden && time_label->hidden, 20);
    press(0);
    CHECK(!strcmp(phase_label->text, "FOCUS") && !strcmp(time_label->text, "25:00"), 21);
    CHECK(pomodoro_timer.active && inside_layout(), 22);
    poll_at(1001); CHECK(!strcmp(time_label->text, "24:59"), 23);
    press(0); CHECK(!strcmp(phase_label->text, "PAUSE") && !pomodoro_timer.active, 24);
    unsigned release_before = callbacks;
    on_pomodoro_released(NULL, (struct zmk_behavior_binding_event){0});
    CHECK(callbacks == release_before, 25);
    poll_at(100000); CHECK(!strcmp(time_label->text, "24:59"), 26);
    press(0); CHECK(pomodoro.running && pomodoro.deadline == 1598999, 27);
    // A blank OLED does not stop the model. At phase end it wakes for 15 seconds.
    activity = ZMK_ACTIVITY_IDLE; display_on = false;
    poll_at(1598999);
    CHECK(!strcmp(phase_label->text, "BREAK") && !strcmp(time_label->text, "05:00"), 28);
    CHECK(display_on && wakes > 0 && alert_active, 29);
    alert_at(1613999); CHECK(!display_on && !alert_active && blanks > 0, 30);
    // A normal key restores native activity; expiry must not blank an active keyboard.
    activity = ZMK_ACTIVITY_ACTIVE; poll_at(1898999);
    CHECK(!strcmp(phase_label->text, "FOCUS") && pomodoro.round == 2, 31);
    alert_at(1913999); CHECK(display_on, 32);
    // Activity-idle racing an alert queues a later unblank, without faking activity.
    poll_at(3398999); activity = ZMK_ACTIVITY_IDLE; display_on = false;
    struct zmk_activity_state_changed idle = {.state = ZMK_ACTIVITY_IDLE};
    pomodoro_activity_cb(&idle);
    CHECK(pomodoro_alert_work.active && pomodoro_alert_work.due == now + 1, 33);
    alert_at(now + 1); CHECK(display_on && activity == ZMK_ACTIVITY_IDLE, 34);
    press(1); CHECK(pomodoro.phase == NANO35_POMO_OFF && phase_label->hidden && !alert_active, 35);
    CHECK(!pomodoro_alert_work.active && !pomodoro_timer.active, 36);
    // The display can initialize after a combo: snapshot restores the running clock.
    display_initialized = false; now = 0; press(0);
    display_initialized = true; nano35_pomodoro_ui_changed(false); drain();
    CHECK(!strcmp(time_label->text, "25:00"), 37);
    poll_at(6900000);
    CHECK(!strcmp(phase_label->text, "DONE") && !strcmp(time_label->text, "00:00"), 38);
    CHECK(!pomodoro_timer.active && inside_layout(), 39);
    press(0); CHECK(pomodoro.round == 1 && pomodoro.phase == NANO35_POMO_FOCUS, 40);
    struct zmk_behavior_binding invalid = {.param1 = 2};
    CHECK(on_pomodoro_pressed(&invalid, (struct zmk_behavior_binding_event){0}) == -EINVAL, 41);
    CHECK(!failure && !lock_depth, 42);
    return 0;
}

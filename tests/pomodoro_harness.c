// SPDX-License-Identifier: MIT
// Included after the actual model, behavior and widget C by test-pomodoro.py.
#define CHECK(condition, code) do { if (!(condition)) return code; } while (0)
static void drain(void) {
    for (unsigned guard=0; queue_count && guard<32; guard++) {
        struct k_work *work=queued[0];
        for(unsigned i=1;i<queue_count;i++)queued[i-1]=queued[i];
        queue_count--;work->pending=false;
        current_queue=work->queue==&display_queue?1:0;work->handler(work);current_queue=0;
    }
    if(queue_count)failure=90;
}
static void press(unsigned minutes) {
    struct zmk_behavior_binding binding={.param1=minutes};
    on_pomodoro_pressed(&binding,(struct zmk_behavior_binding_event){0});drain();
}
static void poll_at(int64_t time) {now=time;pomodoro_tick(&pomodoro_timer);drain();}
static void alert_at(int64_t time) {now=time;current_queue=1;pomodoro_alert_cb(&pomodoro_alert_work.work);current_queue=0;}
static bool inside_layout(void) {
    for(unsigned i=1;i<object_count;i++) {
        lv_obj_t *o=&objects[i];
        if(!o->hidden && (o->x<0 || o->y<18 || o->x+o->w>74 || o->y+o->h>36))return false;
    }
    return true;
}
static bool hidden_timer(void) {
    return phase_label->hidden && time_label->hidden && party_icon->hidden && confetti_icon->hidden;
}
int main(void) {
    const uint32_t presets[]={5,10,25};
    // All three presets: every second to expiry, 64-bit uptime, exact 10+60s window.
    for(unsigned p=0;p<3;p++) {
        struct nano35_pomo_state s={0};int64_t start=INT64_C(5000000000);
        CHECK(nano35_pomo_command(&s,presets[p],start),1);
        CHECK(nano35_pomo_seconds(&s,start)==presets[p]*60,2);
        CHECK(nano35_pomo_seconds(&s,start+1)==presets[p]*60,3);
        for(int64_t t=start+1000;t<s.deadline;t+=1000) {
            CHECK(!nano35_pomo_advance(&s,t) && s.phase==NANO35_POMO_RUNNING,4);
        }
        int64_t deadline=s.deadline;
        CHECK(nano35_pomo_advance(&s,deadline) && s.phase==NANO35_POMO_DONE && s.done_at==deadline,5);
        CHECK(nano35_pomo_seconds(&s,deadline)==0,6);
        CHECK(!nano35_pomo_advance(&s,deadline+69999) && s.phase==NANO35_POMO_DONE,7);
        CHECK(nano35_pomo_advance(&s,deadline+70000) && s.phase==NANO35_POMO_OFF,8);
        // A delayed callback never starts a fresh notification window.
        nano35_pomo_command(&s,presets[p],start);
        nano35_pomo_advance(&s,deadline+70001);CHECK(s.phase==NANO35_POMO_OFF,9);
    }
    struct nano35_pomo_state s={0};
    nano35_pomo_command(&s,25,0);nano35_pomo_command(&s,10,1234);
    CHECK(s.deadline==601234 && s.phase==NANO35_POMO_RUNNING,10);
    nano35_pomo_command(&s,10,2000);CHECK(s.deadline==602000,11);
    CHECK(!nano35_pomo_command(&s,1,2001) && s.deadline==602000,12);
    nano35_pomo_command(&s,0,2500);nano35_pomo_advance(&s,9000000);CHECK(s.phase==NANO35_POMO_OFF,13);
    // Actual behavior + widget, with LVGL/driver calls checked for display-queue context.
    current_queue=1;nano35_widget_pomodoro_init(&objects[object_count++]);current_queue=0;
    CHECK(hidden_timer(),20);
    press(5);CHECK(!strcmp(phase_label->text,"TIMER") && !strcmp(time_label->text,"05:00"),21);
    CHECK(pomodoro_timer.active && party_icon->hidden && confetti_icon->hidden && inside_layout(),22);
    poll_at(1000);CHECK(!strcmp(time_label->text,"04:59"),23);
    now=1234;press(10);CHECK(!strcmp(time_label->text,"10:00") && pomodoro.deadline==601234,24);
    now=2000;press(10);CHECK(pomodoro.deadline==602000 && !strcmp(time_label->text,"10:00"),25);
    now=2500;press(25);CHECK(pomodoro.deadline==1502500 && !strcmp(time_label->text,"25:00"),26);
    unsigned before=callbacks;on_pomodoro_released(NULL,(struct zmk_behavior_binding_event){0});
    CHECK(callbacks==before,27);
    struct zmk_behavior_binding invalid={.param1=1};
    CHECK(on_pomodoro_pressed(&invalid,(struct zmk_behavior_binding_event){0})==-EINVAL && pomodoro.deadline==1502500,28);
    press(0);CHECK(hidden_timer() && !pomodoro_timer.active,29);
    // Native 5-minute blanking can coincide with expiry. Whole UI wakes and blinks.
    now=0;press(5);activity=ZMK_ACTIVITY_IDLE;display_on=false;poll_at(300000);
    CHECK(!strcmp(phase_label->text,"DONE") && !strcmp(time_label->text,"00:00"),30);
    CHECK(!party_icon->hidden && !confetti_icon->hidden && inside_layout(),31);
    for(int i=0;i<20;i++) {
        alert_at(300000+i*500);
        CHECK(display_on==(i%2==0) && alert_active && pomodoro_alert_work.due==now+500,32);
    }
    alert_at(309999);CHECK(!display_on && pomodoro_alert_work.due==310000,33);
    alert_at(310000);CHECK(display_on && alert_active,34);
    alert_at(369999);CHECK(display_on && !hidden_timer() && pomodoro_alert_work.due==370000,35);
    alert_at(370000);CHECK(hidden_timer() && !display_on && !alert_active,36);
    poll_at(370000);CHECK(pomodoro.phase==NANO35_POMO_OFF && !pomodoro_timer.active,37);
    // Restart during an off half-cycle restores the panel and cancels old deadlines.
    now=0;activity=ZMK_ACTIVITY_ACTIVE;press(5);poll_at(300000);alert_at(300500);
    CHECK(!display_on,38);
    now=300750;press(10);CHECK(display_on && !alert_active && !pomodoro_alert_work.active,39);
    CHECK(pomodoro.deadline==900750 && !strcmp(time_label->text,"10:00") && party_icon->hidden,40);
    alert_at(301000);poll_at(370000);CHECK(pomodoro.phase==NANO35_POMO_RUNNING && !hidden_timer(),41);
    // Cancel mid-blink, then deliver stale callbacks: no flash or state resurrection.
    now=0;press(5);poll_at(300000);alert_at(300500);press(0);
    CHECK(display_on && hidden_timer() && !alert_active && !pomodoro_alert_work.active,42);
    alert_at(301000);poll_at(370000);CHECK(hidden_timer() && !pomodoro_timer.active,43);
    // A keyboard event in an off half-cycle must not accidentally stop blinking.
    now=0;press(5);poll_at(300000);alert_at(300500);now=300750;display_on=true;
    struct zmk_activity_state_changed event={.state=ZMK_ACTIVITY_ACTIVE};
    pomodoro_activity_cb(&event);CHECK(pomodoro_alert_work.due==now+1,44);
    alert_at(now+1);CHECK(!display_on && activity==ZMK_ACTIVITY_ACTIVE,45);
    // Keep DONE visible during the extra minute, despite native idle blanking.
    alert_at(315000);activity=ZMK_ACTIVITY_IDLE;display_on=false;event.state=ZMK_ACTIVITY_IDLE;
    pomodoro_activity_cb(&event);alert_at(now+1);CHECK(display_on && activity==ZMK_ACTIVITY_IDLE,46);
    // Active input at automatic hide leaves the normal UI lit.
    activity=ZMK_ACTIVITY_ACTIVE;alert_at(370000);CHECK(hidden_timer() && display_on,47);
    // Late initialization and a UI callback long after expiry do not resurrect DONE.
    display_initialized=false;now=0;press(25);display_initialized=true;nano35_pomodoro_ui_changed(false);drain();
    CHECK(!strcmp(time_label->text,"25:00"),48);
    alert_at(2000000);CHECK(hidden_timer() && !alert_active,49);
    poll_at(2000000);CHECK(!pomodoro_timer.active,50);
    CHECK(!failure && !lock_depth,51);
    return 0;
}

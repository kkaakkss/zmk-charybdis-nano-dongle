// SPDX-License-Identifier: MIT
#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include <stdarg.h>
#include "pomodoro.h"
#ifdef POMO_ARM_TEST
void *memset(void *p,int v,size_t n) {unsigned char *d=p;while(n--)*d++=v;return p;}
void *memcpy(void *p,const void *s,size_t n) {unsigned char *d=p;const unsigned char *a=s;while(n--)*d++=*a++;return p;}
size_t strlen(const char *s) {size_t n=0;while(s[n])n++;return n;}
int strcmp(const char *a,const char *b) {while(*a&&*a==*b){a++;b++;}return (unsigned char)*a-(unsigned char)*b;}
int snprintf(char *out,size_t size,const char *format,...) {
    va_list args;va_start(args,format);unsigned m=va_arg(args,unsigned),s=va_arg(args,unsigned);va_end(args);
    if(size!=6 || strcmp(format,"%02u:%02u") || m>99 || s>59)return -1;
    out[0]='0'+m/10;out[1]='0'+m%10;out[2]=':';out[3]='0'+s/10;out[4]='0'+s%10;out[5]=0;return 5;
}
#else
#include <stdio.h>
#include <string.h>
#endif
#define BUILD_ASSERT(cond, ...) _Static_assert(cond, #cond)
#define ARRAY_SIZE(x) (sizeof(x)/sizeof((x)[0]))
#define CONFIG_ZMK_BEHAVIOR_METADATA 0
#define CONFIG_ZMK_DISPLAY_BLANK_ON_IDLE 1
#define IS_ENABLED(x) (x)
#define DT_NUM_INST_STATUS_OKAY(x) 1
#define DT_INST_PROP(i,p) PROP_##p
#define PROP_focus_minutes 25
#define PROP_break_minutes 5
#define PROP_rounds 4
#define K_FOREVER 0
#define K_SECONDS(n) ((n)*1000)
#define K_MSEC(n) (n)
#define EINVAL 22
#define ZMK_BEHAVIOR_OPAQUE 0
#define BEHAVIOR_LOCALITY_CENTRAL 0
#define BEHAVIOR_DT_INST_DEFINE(...)
#define ZMK_LISTENER(...)
#define ZMK_SUBSCRIPTION(...)
struct zmk_behavior_binding {unsigned param1;};
struct zmk_behavior_binding_event {int unused;};
struct behavior_driver_api {int locality;int(*binding_pressed)(struct zmk_behavior_binding*,struct zmk_behavior_binding_event);int(*binding_released)(struct zmk_behavior_binding*,struct zmk_behavior_binding_event);};
static int64_t now;
static unsigned failure,lock_depth,current_queue,callbacks,wakes,blanks;
static bool display_on=true,display_initialized=true;
static int64_t k_uptime_get(void) {return now;}
#define K_MUTEX_DEFINE(n) int n
static int k_mutex_lock(int *m,int t) {if(lock_depth++)failure=80;return 0;}
static int k_mutex_unlock(int *m) {if(--lock_depth)failure=81;return 0;}
struct k_work_q {int unused;};
static struct k_work_q display_queue,system_queue;
struct k_work {void(*handler)(struct k_work*);bool pending;struct k_work_q *queue;};
struct k_work_delayable {struct k_work work;bool active;int64_t due;};
struct k_timer {void(*handler)(struct k_timer*);bool active;};
#define K_WORK_DEFINE(n,cb) struct k_work n={.handler=cb}
#define K_WORK_DELAYABLE_DEFINE(n,cb) struct k_work_delayable n={.work={.handler=cb}}
#define K_TIMER_DEFINE(n,cb,stop) struct k_timer n={.handler=cb}
static struct k_work *queued[32];static unsigned queue_count;
static int k_work_submit_to_queue(struct k_work_q *q,struct k_work *w) {if(!w->pending){if(queue_count>=32){failure=82;return 0;}w->pending=true;w->queue=q;queued[queue_count++]=w;}return 0;}
static int k_work_submit(struct k_work *w) {return k_work_submit_to_queue(&system_queue,w);}
static int k_work_reschedule_for_queue(struct k_work_q *q,struct k_work_delayable *w,int delay) {w->work.queue=q;w->active=true;w->due=now+delay;return 0;}
static int k_work_cancel_delayable(struct k_work_delayable *w) {w->active=false;return 0;}
static void k_timer_start(struct k_timer *t,int first,int period) {t->active=true;}
static void k_timer_stop(struct k_timer *t) {t->active=false;}
static struct k_work_q *zmk_display_work_q(void) {return &display_queue;}
static bool zmk_display_is_initialized(void) {return display_initialized;}
typedef int atomic_t;
static int atomic_get(atomic_t *p) {return *p;}
static int atomic_set(atomic_t *p,int value) {int old=*p;*p=value;return old;}
enum {ZMK_ACTIVITY_ACTIVE,ZMK_ACTIVITY_IDLE,ZMK_ACTIVITY_SLEEP};
static int activity=ZMK_ACTIVITY_ACTIVE;
static int zmk_activity_get_state(void) {return activity;}
struct zmk_activity_state_changed {int state;};
typedef struct zmk_activity_state_changed zmk_event_t;
static const struct zmk_activity_state_changed *as_zmk_activity_state_changed(const zmk_event_t *e) {return e;}
#define ZMK_EV_EVENT_BUBBLE 0
void unblank_display_cb(struct k_work *work) {if(current_queue!=1)failure=83;display_on=true;wakes++;}
void blank_display_cb(struct k_work *work) {if(current_queue!=1)failure=84;display_on=false;blanks++;}
typedef struct {int x,y,w,h;bool hidden;char text[16];} lv_obj_t;
static lv_obj_t objects[16];static unsigned object_count;
#define LV_OBJ_FLAG_HIDDEN 1
#define LV_PART_MAIN 0
#define LV_OPA_COVER 255
static void ui_check(void) {callbacks++;if(current_queue!=1 || lock_depth)failure=85;}
static lv_obj_t *lv_label_create(lv_obj_t *p) {ui_check();return &objects[object_count++];}
static lv_obj_t *lv_obj_create(lv_obj_t *p) {return lv_label_create(p);}
static void lv_obj_set_pos(lv_obj_t *o,int x,int y) {ui_check();o->x=x;o->y=y;}
static void lv_obj_set_size(lv_obj_t *o,int w,int h) {ui_check();o->w=w;o->h=h;}
static void lv_obj_add_flag(lv_obj_t *o,int flag) {ui_check();o->hidden=true;}
static void lv_obj_clear_flag(lv_obj_t *o,int flag) {ui_check();o->hidden=false;}
static const char *lv_label_get_text(lv_obj_t *o) {ui_check();return o->text;}
static void lv_label_set_text(lv_obj_t *o,const char *text) {ui_check();size_t n=strlen(text);if(n>=sizeof(o->text)){failure=86;return;}memcpy(o->text,text,n+1);o->w=n?9*n-1:0;o->h=9;}
static void lv_obj_remove_style_all(lv_obj_t *o) {ui_check();}
static int lv_obj_get_style_text_color(lv_obj_t *o,int part) {ui_check();return 0;}
static void lv_obj_set_style_bg_color(lv_obj_t *o,int color,int part) {ui_check();}
static void lv_obj_set_style_bg_opa(lv_obj_t *o,int opacity,int part) {ui_check();}

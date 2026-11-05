// SPDX-License-Identifier: MIT
#include <lvgl.h>
#include <zephyr/kernel.h>
#include <zmk/display.h>
#include "trackball_status.h"
#include "speed_status.h"

static lv_obj_t *speed_root, *speed_unit, *speed_value;
// 4 glyphs including letter spacing fit in 35px. The battery begins at x=77.
enum { SPEED_X=38, SPEED_Y=0, SPEED_WIDTH=36, SPEED_HEIGHT=18 };
static void speed_text(char out[6], uint16_t value, bool percent) {
    char reversed[5]; unsigned n=0, i=0;
    do { reversed[n++]='0'+value%10; value/=10; } while(value && n<5);
    while(n) { out[i++]=reversed[--n]; }
    if(percent && i<5) { out[i++]='%'; }
    out[i]=0;
}
static void speed_update(struct k_work *work) {
    if(!speed_root) { return; }
    struct nano35_speed_status state;
    nano35_trackball_speed_snapshot(&state);
    if(!state.visible) { lv_obj_add_flag(speed_root,LV_OBJ_FLAG_HIDDEN); return; }
    char text[6]; speed_text(text,state.value,state.scroll);
    lv_label_set_text(speed_unit,state.scroll ? "SCR" : "eCPI");
    lv_label_set_text(speed_value,text);
    lv_obj_clear_flag(speed_root,LV_OBJ_FLAG_HIDDEN);
}
K_WORK_DEFINE(speed_work,speed_update);
void nano35_trackball_speed_ui_changed(void) {
    if(zmk_display_is_initialized()) {
        k_work_submit_to_queue(zmk_display_work_q(),&speed_work);
    }
}
void nano35_widget_speed_init(lv_obj_t *parent) {
    speed_root=lv_obj_create(parent);
    lv_obj_remove_style_all(speed_root);
    lv_obj_clear_flag(speed_root,LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_size(speed_root,SPEED_WIDTH,SPEED_HEIGHT);
    lv_obj_set_pos(speed_root,SPEED_X,SPEED_Y);
    speed_unit=lv_label_create(speed_root); speed_value=lv_label_create(speed_root);
    lv_obj_t *labels[]={speed_unit,speed_value};
    for(unsigned i=0;i<2;i++) {
        lv_obj_set_width(labels[i],SPEED_WIDTH);
        lv_label_set_long_mode(labels[i],LV_LABEL_LONG_CLIP);
        lv_obj_set_style_text_align(labels[i],LV_TEXT_ALIGN_RIGHT,0);
        lv_obj_set_pos(labels[i],0,i*9);
    }
    speed_update(NULL);
}

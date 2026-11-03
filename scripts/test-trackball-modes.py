#!/usr/bin/env python3
"""Actual Nano processor + pinned ZMK listener C: ARM emulation or host UBSan.

Kernel locks/layer scheduling and HID/USB endpoints are doubles. Sensor/BLE,
real timing, RF, optical resolution and device ergonomics are not simulated.
"""
from pathlib import Path
import argparse,subprocess,tempfile

p=argparse.ArgumentParser(description=__doc__)
p.add_argument('--workspace',type=Path,required=True)
compiler=p.add_mutually_exclusive_group(required=True)
compiler.add_argument('--arm-gcc',type=Path)
compiler.add_argument('--host-cc',help='Native GCC/Clang with UBSan (for Linux CI)')
a=p.parse_args()
repo=Path(__file__).resolve().parents[1]; ws=a.workspace.resolve()

def function(s,signature):
    start=s.index(signature); end=s.index('{',start)+1; depth=1
    while depth:
        depth+=(s[end]=='{')-(s[end]=='}'); end+=1
    return s[start:end]

processor=(repo/'src/trackball_modes.c').read_text(encoding='utf-8')
listener=(ws/'zmk/app/src/pointing/input_listener.c').read_text(encoding='utf-8')
header=r'''
#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include "trackball_math.h"
void *memset(void *dest,int value,size_t count) {
    unsigned char *p=dest;while(count--) *p++=(unsigned char)value;return dest;
}
#define CONFIG_ZMK_POINTING_SMOOTH_SCROLLING 0
#define IS_ENABLED(x) (x)
#define LOG_ERR(...) ((void)0)
#define BIT(x) (1u<<(x))
#define WRITE_BIT(v,n,b) ((v)=(b)?((v)|BIT(n)):((v)&~BIT(n)))
#define K_FOREVER 0
#define INPUT_EV_KEY 1
#define INPUT_EV_REL 2
#define INPUT_EV_ABS 3
#define INPUT_REL_X 0
#define INPUT_REL_Y 1
#define INPUT_REL_WHEEL 8
#define INPUT_REL_HWHEEL 6
#define INPUT_BTN_0 0x100
#define INPUT_BTN_1 0x101
#define INPUT_BTN_2 0x102
#define INPUT_BTN_3 0x103
#define INPUT_BTN_4 0x104
#define ZMK_HID_MOUSE_NUM_BUTTONS 5
#define ZMK_INPUT_PROC_CONTINUE 0
#define ZMK_INPUT_PROC_STOP 1
#define ZMK_EV_EVENT_BUBBLE 0
#define MOUSE_LAYER 4
#define SCROLL_LAYER 5
#define SNIPE_LAYER 6
#define FASTER_LAYER 7
#define AUTO_TIMEOUT 30000
struct device {int unused;};
struct input_event {const struct device *dev; uint8_t type; uint16_t code; int32_t value; bool sync;};
struct zmk_input_processor_state {uint8_t input_device_index; int16_t *remainder;};
struct zmk_input_processor_entry {int unused;};
typedef int zmk_event_t;
static struct device sensor;
static const struct device *temporary_layer=&sensor;
static struct nano35_ball_state ball;
static int ball_lock,lock_depth,highest,auto_calls,failure;
static uint32_t test_now;
static uint32_t k_uptime_get_32(void) {return test_now;}
static int k_mutex_lock(int *l,int timeout) {if(lock_depth) failure=90;lock_depth++;return 0;}
static int k_mutex_unlock(int *l) {lock_depth--;return 0;}
static int zmk_keymap_highest_layer_active(void) {return highest;}
static int zmk_input_processor_handle_event(const struct device *dev,struct input_event *event,uint32_t layer,uint32_t timeout,struct zmk_input_processor_state *state) {
    if(lock_depth || layer!=4 || timeout!=30000) failure=91;
    auto_calls++;return 0;
}
'''
parts=[header]
parts += [function(processor,s) for s in ['static enum nano35_ball_mode current_mode(', 'static int modes_layer_event(', 'static int modes_handle_event(']]
parts += [function(processor, 'void nano35_trackball_speed_step(')]
parts += [r'''
#define CONFIG_ZMK_SPLIT_ROLE_CENTRAL 1
#define ZMK_BEHAVIOR_OPAQUE 0
#define EINVAL 22
struct zmk_behavior_binding { uint32_t param1; };
struct zmk_behavior_binding_event { int unused; };
''']
speed=(repo/'src/ball_speed.c').read_text()
parts += [function(speed, signature) for signature in ['static int speed_pressed(', 'static int speed_released(']]
parts += [listener[listener.index('enum input_listener_xy_data_mode'):listener.index('static void handle_rel_code')]]
parts += [r'''
static int16_t hid_x,hid_y,wheel_x,wheel_y;
static int32_t total_x,total_y,total_wx,total_wy;
static unsigned buttons,reports;
static int filter_with_input_config(const struct input_listener_config *cfg,struct input_listener_data *data,struct input_event *event) {
    return modes_handle_event(&sensor,event,0,0,NULL);
}
static void zmk_hid_mouse_movement_set(int16_t x,int16_t y) {hid_x=x;hid_y=y;}
static void zmk_hid_mouse_scroll_set(int16_t x,int16_t y) {wheel_x=x;wheel_y=y;}
static void zmk_hid_mouse_button_press(int b) {buttons|=BIT(b);}
static void zmk_hid_mouse_button_release(int b) {buttons&=~BIT(b);}
static int zmk_endpoints_send_mouse_report(void) {reports++;total_x+=hid_x;total_y+=hid_y;total_wx+=wheel_x;total_wy+=wheel_y;return 0;}
''']
parts += [function(listener,s) for s in ['static void handle_rel_code(', 'static void handle_abs_code(', 'static void handle_key_code(', 'static void clear_xy_data(', 'static void input_handler(']]
# Pinned ZMK combo release logic: API/active combo storage are doubles.
parts += [r'''
struct combo_cfg { int16_t key_position_len; bool slow_release; };
struct active_combo { unsigned combo_idx,key_positions_pressed_count; struct {struct {int32_t position;} data;} key_positions_pressed[2]; };
static struct combo_cfg combos[1]={{.key_position_len=2}};
static struct active_combo active_combos[1];
static int active_combo_count,combo_release_calls;
static int release_combo_behavior(int index,const struct combo_cfg *c,int64_t timestamp) {
    combo_release_calls++;highest=4;modes_layer_event(NULL);return 0;
}
static void deactivate_combo(int index) {active_combo_count--;}
''']
parts += [function((ws/'zmk/app/src/combo.c').read_text(), 'static bool release_combo_key(')]
parts += [r'''
static struct input_listener_config cfg;
static struct input_listener_data data;
static void layer(int n) {highest=n;modes_layer_event(NULL);}
static void frame(int x,int y) {
    test_now+=8;
    struct input_event ex={&sensor,INPUT_EV_REL,INPUT_REL_X,x,false};
    struct input_event ey={&sensor,INPUT_EV_REL,INPUT_REL_Y,y,true};
    input_handler(&cfg,&data,&ex);input_handler(&cfg,&data,&ey);
    if(hid_x || hid_y || wheel_x || wheel_y) failure=92;
}
static void button(int b,bool pressed) {
    struct input_event e={&sensor,INPUT_EV_KEY,INPUT_BTN_0+b,pressed,true};
    input_handler(&cfg,&data,&e);
}
int main(void) {
    // All actual post-orientation 12-bit values, including +2048.
    for(int mode=1;mode<=2;mode++) for(int axis=0;axis<2;axis++) {
        for(int v=-2048;v<=2048;v++) {
            struct nano35_ball_state s={0};nano35_ball_set_mode(&s,mode);
            int div=mode==NANO35_SNIPE?4:8;
            int result=nano35_ball_axis(&s,axis,v);
            if(mode==NANO35_SCROLL && axis==1) result=-result;
            if(result!=v/div || result*div*100+s.remainder[axis]!=v*100) return 1;
        }
    }
    layer(0);frame(2,3);if(auto_calls) return 2;
    frame(3,3);if(auto_calls!=1) return 3;
    for(int n=1;n<=3;n++) {layer(n);frame(100,100);}
    if(auto_calls!=1) return 4; // Keyboard layers aren't automatically masked.
    layer(4);frame(10,0);if(auto_calls!=2) return 5;
    int bx=total_x,by=total_y,wx=total_wx,wy=total_wy;
    layer(5);frame(7,-7);
    if(total_wx!=wx || total_wy!=wy || total_x!=bx || total_y!=by) return 6;
    frame(1,-1);if(total_wx-wx!=1 || total_wy-wy!=1) return 7;
    frame(-8,8);if(total_wx!=wx || total_wy!=wy) return 8;
    // Both axes accumulate independently. No orthogonal-axis reset.
    for(int i=0;i<8;i++) frame(1,1);
    if(total_wx-wx!=1 || total_wy-wy!=-1) return 9;
    // Leave and re-enter without any movement in between: clear stale fractions.
    frame(7,7);layer(4);layer(5);wx=total_wx;wy=total_wy;frame(1,1);
    if(total_wx!=wx || total_wy!=wy) return 10;
    layer(6);bx=total_x;by=total_y;
    for(int i=0;i<4;i++) frame(1,-1);
    if(total_x-bx!=1 || total_y-by!=-1) return 11;
    frame(3,3);layer(4);layer(6);bx=total_x;by=total_y;frame(1,1);
    if(total_x!=bx || total_y!=by) return 12;
    // Press/drag across every mode, then release after returning to base.
    for(int b=0;b<3;b++) {
        layer(4);button(b,true);frame(20,-20);
        layer(5);frame(16,-16);layer(6);frame(4,-4);layer(0);button(b,false);
        if(buttons) return 13;
    }
    unsigned before=reports;frame(0,0);
    if(reports-before!=1) return 14; // One final sync, never two axis reports.
    // After a pause, the first report is unaccelerated, even for a large delta.
    layer(0);test_now+=100;bx=total_x;by=total_y;frame(-2048,2048);
    if(total_x-bx!=-2048 || total_y-by!=2048) return 15;
    bx=total_x;by=total_y;wx=total_wx;wy=total_wy;
    for(int i=0;i<100;i++) frame(0,0);
    if(total_x!=bx || total_y!=by || total_wx!=wx || total_wy!=wy) return 16;
    // Slow samples remain exactly 1x; high speed is capped at 1.75x on BOTH axes.
    test_now+=100;bx=total_x;by=total_y;
    for(int i=0;i<100;i++) frame(2,-1);
    if(total_x-bx!=200 || total_y-by!=-100 || ball.accel_gain!=256) return 17;
    for(int i=0;i<50;i++) frame(16,-8);
    if(ball.accel_gain!=448) return 18;
    bx=total_x;by=total_y;
    for(int i=0;i<4;i++) frame(16,-8);
    if(total_x-bx!=112 || total_y-by!=-56) return 19;
    // Raw threshold stays unchanged even when output is accelerated.
    int calls=auto_calls;frame(2,3);if(auto_calls!=calls) return 20;
    // No gain leaks into Scroll/Snipe or back across mode changes.
    layer(5);wx=total_wx;wy=total_wy;frame(8,-8);
    if(total_wx-wx!=1 || total_wy-wy!=1) return 21;
    layer(6);bx=total_x;by=total_y;frame(4,-4);
    if(total_x-bx!=1 || total_y-by!=-1) return 22;
    layer(0);bx=total_x;by=total_y;frame(16,-8);
    if(total_x-bx!=16 || total_y-by!=-8 || ball.accel_gain!=256) return 23;
    // Same physical speed at different reporting rates has the same stable gain.
    for(int interval=8;interval<=16;interval+=8) {
        struct nano35_ball_state s={0};uint32_t now=0xfffffff0u;
        for(int i=0;i<50;i++) {
            now+=interval;nano35_ball_begin_frame(&s,now);
            nano35_ball_axis(&s,0,interval);nano35_ball_axis(&s,1,0);
            nano35_ball_end_frame(&s);
        }
        if(s.accel_gain!=352) return 24; // 1000 counts/s -> 1.375x, across uptime wrap.
        // A BLE burst with equal timestamps never divides by zero or exceeds cap.
        for(int i=0;i<50;i++) {
            nano35_ball_begin_frame(&s,now);nano35_ball_axis(&s,0,2048);
            nano35_ball_end_frame(&s);
        }
        if(s.accel_gain>448) return 25;
        nano35_ball_begin_frame(&s,now+100);
        if(nano35_ball_axis(&s,0,1)!=1 || s.accel_gain!=256) return 26;
        nano35_ball_end_frame(&s);
    }
    // One-axis sensor reports close their own frame and emit exactly one report.
    test_now+=100;bx=total_x;by=total_y;before=reports;
    struct input_event solo={&sensor,INPUT_EV_REL,INPUT_REL_X,2,true};
    input_handler(&cfg,&data,&solo);
    test_now+=8;solo.code=INPUT_REL_Y;solo.value=-3;
    input_handler(&cfg,&data,&solo);
    if(total_x-bx!=2 || total_y-by!=-3 || reports-before!=2 || ball.frame_open) return 27;
    // Real button behavior: press once, release never repeats; preserve mode settings.
    struct zmk_behavior_binding up={.param1=1},down={.param1=0},bad={.param1=6};
    struct zmk_behavior_binding_event event={0};
    layer(4);test_now+=100;
    speed_pressed(&up,event); if(ball.speed_percent!=110) return 28;
    speed_released(&up,event); if(ball.speed_percent!=110) return 29;
    bx=total_x;by=total_y;frame(100,-100);
    if(total_x-bx!=110 || total_y-by!=-110) return 30;
    speed_pressed(&down,event);if(ball.speed_percent!=100) return 31;
    for(int i=0;i<40;i++) speed_pressed(&down,event);
    if(ball.speed_percent!=20) return 32;
    // Fractional low-speed motion is retained, including negative values.
    test_now+=100;bx=total_x;by=total_y;
    for(int i=0;i<5;i++) frame(1,-1);
    if(total_x-bx!=1 || total_y-by!=-1) return 33;
    for(int i=0;i<40;i++) speed_pressed(&up,event);
    if(ball.speed_percent!=200 || speed_pressed(&bad,event)!=-EINVAL) return 34;
    layer(5);wx=total_wx;wy=total_wy;frame(8,-8);
    if(total_wx-wx!=1 || total_wy-wy!=1 || ball.speed_percent!=200) return 35;
    layer(6);bx=total_x;by=total_y;frame(4,-4);
    if(total_x-bx!=1 || total_y-by!=-1) return 36;
    layer(4);if(ball.speed_percent!=200) return 37;
    // Button changes reset acceleration fractions; threshold still sees raw counts.
    speed_pressed(&down,event);if(ball.accel_remainder[0] || ball.accel_remainder[1]) return 38;
    calls=auto_calls;frame(2,3);if(auto_calls!=calls) return 39;
    // Explicit per-mode actions must not touch the other two multipliers.
    struct zmk_behavior_binding scroll_up={.param1=3},scroll_down={.param1=2};
    struct zmk_behavior_binding snipe_up={.param1=5},snipe_down={.param1=4};
    unsigned move_percent=ball.speed_percent;
    layer(5);speed_pressed(&scroll_up,event);
    if(ball.scroll_percent!=110 || ball.speed_percent!=move_percent || ball.snipe_percent) return 40;
    for(int i=0;i<40;i++) speed_pressed(&scroll_down,event);
    if(ball.scroll_percent!=20) return 41;
    wx=total_wx;wy=total_wy;
    for(int i=0;i<40;i++) frame(1,-1);
    if(total_wx-wx!=1 || total_wy-wy!=1) return 42;
    for(int i=0;i<40;i++) speed_pressed(&scroll_up,event);
    if(ball.scroll_percent!=200) return 43;
    wx=total_wx;wy=total_wy;frame(-8,8);
    if(total_wx-wx!=-2 || total_wy-wy!=-2) return 44;
    layer(6);speed_pressed(&snipe_up,event);
    if(ball.snipe_percent!=110 || ball.scroll_percent!=200 || ball.speed_percent!=move_percent) return 45;
    for(int i=0;i<40;i++) speed_pressed(&snipe_down,event);
    if(ball.snipe_percent!=20) return 46;
    bx=total_x;by=total_y;
    for(int i=0;i<20;i++) frame(1,-1);
    if(total_x-bx!=1 || total_y-by!=-1) return 47;
    for(int i=0;i<40;i++) speed_pressed(&snipe_up,event);
    if(ball.snipe_percent!=200) return 48;
    bx=total_x;by=total_y;frame(-4,4);
    if(total_x-bx!=-2 || total_y-by!=2) return 49;
    layer(4);layer(5);layer(6);
    if(ball.speed_percent!=move_percent || ball.scroll_percent!=200 || ball.snipe_percent!=200) return 50;
    // FASTER is normal movement x1.5, preserving signed fractions and both axes.
    ball.speed_percent=100;layer(7);test_now+=100;bx=total_x;by=total_y;
    wx=total_wx;wy=total_wy;frame(100,-100);
    if(ball.mode!=NANO35_FAST || total_x-bx!=150 || total_y-by!=-150) return 51;
    if(total_wx!=wx || total_wy!=wy) return 52;
    layer(4);layer(7);test_now+=100;bx=total_x;by=total_y;
    for(int i=0;i<10;i++) frame(1,-1);
    if(total_x-bx!=15 || total_y-by!=-15) return 53;
    // The original time-based acceleration curve is used unchanged in FASTER.
    for(int i=0;i<50;i++) frame(16,-8);
    if(ball.accel_gain!=448) return 54;
    bx=total_x;by=total_y;
    for(int i=0;i<4;i++) frame(16,-8);
    if(total_x-bx!=168 || total_y-by!=-84) return 55;
    calls=auto_calls;frame(2,3);if(auto_calls!=calls) return 56;
    frame(10,0);if(auto_calls!=calls+1) return 57;
    // Inherit current normal multiplier; no separate persistent FASTER setting.
    layer(4);ball.speed_percent=200;layer(7);test_now+=100;bx=total_x;by=total_y;frame(10,-10);
    if(total_x-bx!=30 || total_y-by!=-30) return 58;
    // Returning to MOUSE clears FASTER gain/fractions; clicks release across modes.
    layer(4);test_now+=100;bx=total_x;by=total_y;frame(10,-10);
    if(total_x-bx!=20 || total_y-by!=-20 || ball.mode!=NANO35_MOVE) return 59;
    for(int b=0;b<3;b++) {layer(7);button(b,true);layer(4);button(b,false);if(buttons) return 60;}
    // Actual ZMK fast-release combo logic: either key releases FASTER exactly once.
    for(int order=0;order<2;order++) {
        active_combo_count=1;combo_release_calls=0;
        active_combos[0].combo_idx=0;active_combos[0].key_positions_pressed_count=2;
        active_combos[0].key_positions_pressed[0].data.position=2;
        active_combos[0].key_positions_pressed[1].data.position=3;
        layer(7);
        if(release_combo_key(19,0) || highest!=7) return 61;
        if(!release_combo_key(order?3:2,0) || highest!=4 || combo_release_calls!=1) return 62;
        if(!release_combo_key(order?2:3,0) || combo_release_calls!=1 || active_combo_count) return 63;
    }
    return failure;
}
''']
with tempfile.TemporaryDirectory(prefix='nano-ball-test-') as tmp:
    tmp=Path(tmp); c=tmp/'test.c'; elfpath=tmp/'test.elf'
    c.write_text('\n'.join(parts),encoding='utf-8')
    if a.host_cc:
        native=tmp/'test-native'
        subprocess.run([a.host_cc,'-std=c99','-O1','-g','-fno-builtin','-fsanitize=undefined',
                        '-fno-sanitize-recover=all','-I',str(repo/'src'),str(c),'-o',str(native)],check=True)
        subprocess.run([str(native)],check=True)
    else:
        subprocess.run([str(a.arm_gcc),'-std=c99','-O0','-ffreestanding','-fno-builtin','-nostdlib','-mcpu=cortex-m4','-mthumb','-Wl,-Ttext=0x10000,-e,main','-I',str(repo/'src'),str(c),'-o',str(elfpath),'-lgcc'],check=True)
        from elftools.elf.elffile import ELFFile
        from unicorn import Uc,UC_ARCH_ARM,UC_MODE_THUMB,UC_MODE_MCLASS
        from unicorn.arm_const import UC_ARM_REG_SP,UC_ARM_REG_LR,UC_ARM_REG_R0,UC_ARM_REG_PC
        emu=Uc(UC_ARCH_ARM,UC_MODE_THUMB|UC_MODE_MCLASS)
        emu.mem_map(0x20000000,0x10000);emu.mem_map(0x30000000,0x1000)
        with elfpath.open('rb') as f:
            elf=ELFFile(f); segments=[s for s in elf.iter_segments() if s['p_type']=='PT_LOAD']
            first=min(s['p_vaddr'] for s in segments)&~0xfff
            last=max(s['p_vaddr']+s['p_memsz'] for s in segments)
            emu.mem_map(first,(last-first+0xfff)&~0xfff)
            for seg in segments:
                if seg['p_filesz']: emu.mem_write(seg['p_vaddr'],seg.data())
            entry=elf.header['e_entry']
        emu.reg_write(UC_ARM_REG_SP,0x2000fff0);emu.reg_write(UC_ARM_REG_LR,0x30000001)
        emu.emu_start(entry|1,0x30000000,count=20000000)
        assert emu.reg_read(UC_ARM_REG_PC)==0x30000000,'test did not finish'
        assert emu.reg_read(UC_ARM_REG_R0)==0,f'C test failed case {emu.reg_read(UC_ARM_REG_R0)}'
print('PASS: 16388 signed-axis cases; 1/4 snipe, 1/8 scroll, sign, fractions and mode-reset')
print('PASS: actual processor + actual ZMK listener; XY sync, auto threshold/keyboard-layer guard, 3-button drag/release across modes, no drift')
print('PASS: timed acceleration 1x..1.75x; shared XY gain, slow precision, cap, raw auto threshold, 100ms idle reset, mode isolation, report-rate equivalence, uptime wrap, zero-time bursts, single-axis sync')
print('LIMIT: kernel scheduling/layers/endpoints are doubles; no USB/BLE/I2C hardware simulation')
print('Instrumentation: host UBSan' if a.host_cc else 'Instrumentation: ARM emulation without UBSan')

print('PASS: runtime speed buttons 20..200%, 10-point steps, release no-op, fractional motion, invalid action, raw auto threshold and Scroll/Snipe isolation')

print('PASS: independent Move/Scroll/Snipe speed actions, each 20..200%, signed low-speed fractions, cap, mode retention and no cross-mode changes')

print('PASS: FASTER x1.5 with inherited normal speed/acceleration, signed fractions, XY and wheel isolation, raw auto refresh, return to MOUSE and button releases')

print('PASS: pinned ZMK combo release logic returns from FASTER on either first release, consumes the remaining release and ignores unrelated keys')

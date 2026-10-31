/* Host harness: actual pinned driver/listener functions are injected, not reimplemented. */
#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <limits.h>
#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#define CONFIG_PMW3610_SWAP_XY 1
#define CONFIG_PMW3610_INVERT_X 1
#define CONFIG_PMW3610_INVERT_Y 1
#define CONFIG_PMW3610_SMART_ALGORITHM 1
#define CONFIG_ZMK_POINTING_SMOOTH_SCROLLING 0
#define ZMK_HID_MOUSE_NUM_BUTTONS 5
#define ZMK_INPUT_PROC_STOP 1
#define IS_ENABLED(x) (x)
#define unlikely(x) (x)
#define CLAMP(v,l,h) ((v)<(l)?(l):((v)>(h)?(h):(v)))
#define BIT(n) (1u<<(n))
#define WRITE_BIT(v,n,b) ((v)=(b)?((v)|BIT(n)):((v)&~BIT(n)))
#define LOG_DBG(...) ((void)0)
#define LOG_WRN(...) ((void)0)
#define LOG_ERR(...) ((void)0)
#define K_NO_WAIT 0
#define INPUT_EV_REL 2
#define INPUT_EV_ABS 3
#define INPUT_EV_KEY 1
#define INPUT_REL_X 0
#define INPUT_REL_Y 1
#define INPUT_REL_WHEEL 8
#define INPUT_REL_HWHEEL 6
#define INPUT_BTN_0 0x100
#define INPUT_BTN_1 0x101
#define INPUT_BTN_2 0x102
#define INPUT_BTN_3 0x103
#define INPUT_BTN_4 0x104
struct pixart_data {bool ready; bool sw_smart_flag;};
struct pixart_config {uint8_t evt_type; uint16_t x_input_code,y_input_code;};
struct device {void *data; const void *config;};
struct input_event {const struct device *dev; uint8_t type; uint16_t code; int32_t value; bool sync;};
static struct pixart_data pd={.ready=true};
static struct pixart_config pc={INPUT_EV_REL,INPUT_REL_X,INPUT_REL_Y};
static struct device sensor={&pd,&pc};
static uint8_t burst[7];
static int read_error;
static int64_t clock_ms=100;
static struct input_event events[4];
static int event_count;
static int shutter_value,smart_writes,last_smart_value;
static int pmw3610_write(const struct device *d,uint8_t reg,uint8_t value) {
    assert(reg==0x32);smart_writes++;last_smart_value=value;return 0;
}
static int64_t k_uptime_get(void) {return clock_ms;}
static int pmw3610_read(const struct device *d,int reg,uint8_t *out,int n) {
    if(read_error) return read_error;
    memcpy(out,burst,n);return 0;
}
static int input_report(const struct device *d,uint8_t type,uint16_t code,int32_t value,bool sync,int timeout) {
    assert(event_count<4);
    events[event_count++]=(struct input_event){d,type,code,value,sync};return 0;
}
#include "driver_report.inc"
#include "listener_types.inc"
static int16_t hid_x,hid_y,scroll_x,scroll_y;
static uint8_t buttons;
static int reports;
static int64_t total_x,total_y;
static int filter_with_input_config(const struct input_listener_config *c,struct input_listener_data *d,struct input_event *e) {return 0;}
static void zmk_hid_mouse_movement_set(int16_t x,int16_t y) {hid_x=x;hid_y=y;}
static void zmk_hid_mouse_scroll_set(int16_t x,int16_t y) {scroll_x=x;scroll_y=y;}
static void zmk_hid_mouse_button_press(int i) {buttons|=BIT(i);}
static void zmk_hid_mouse_button_release(int i) {buttons&=~BIT(i);}
static int zmk_endpoints_send_mouse_report(void) {reports++;total_x+=hid_x;total_y+=hid_y;return 0;}
#include "listener_functions.inc"
#include "wire_struct.inc"
static struct input_listener_config lc={0};
static struct input_listener_data ld={0};
static void frame(int x,int y,int dt) {
    assert(x>=-2048&&x<=2047&&y>=-2048&&y<=2047);
    unsigned ux=(unsigned)x&4095,uy=(unsigned)y&4095;
    memset(burst,0,sizeof(burst));burst[1]=ux&255;burst[2]=uy&255;burst[3]=((ux>>8)<<4)|(uy>>8);
    burst[5]=(shutter_value>>8)&1;burst[6]=shutter_value&255;
    event_count=0;clock_ms+=dt;assert(pmw3610_report_data(&sensor)==0);
    for(int i=0;i<event_count;i++) {
        struct input_event e=events[i];
        /* Preserve the real split payload layout, including its unsigned value field. */
        struct zmk_split_input_event_payload tx={.type=e.type,.code=e.code,.value=e.value,.sync=e.sync},rx;
        unsigned char wire[sizeof(tx)];memcpy(wire,&tx,sizeof(tx));memcpy(&rx,wire,sizeof(rx));
        e.type=rx.type;e.code=rx.code;e.value=rx.value;e.sync=rx.sync;
        input_handler(&lc,&ld,&e);
    }
    assert(hid_x==0&&hid_y==0&&scroll_x==0&&scroll_y==0);
}
int main(int argc,char **argv) {
    _Static_assert(sizeof(struct zmk_split_input_event_payload)==8,"split payload ABI");
    if(argc>1&&strcmp(argv[1],"legacy")==0) {
        frame(3,4,0);frame(1,2,4);frame(0,0,100);
        assert(total_x==-4&&total_y==-3); /* Expected defect: should be -6/-4. */
        printf("REPRODUCED: 12ms limiter lost final delta (expected -6/-4, got %lld/%lld)\n",(long long)total_x,(long long)total_y);return 0;
    }
    frame(3,4,0);frame(1,2,4);frame(0,0,100);assert(total_x==-6&&total_y==-4);
    int cases=3;
    for(int x=-2048;x<=2047;x++) {
        int y=(((x+2048)*31)&4095)-2048;
        int64_t bx=total_x,by=total_y;int before=reports;
        frame(x,y,1);assert(total_x-bx==-y&&total_y-by==-x);
        assert(reports-before==((x!=0||y!=0)?1:0));cases++;
    }
    int edge[]={-2048,-1,0,1,2047};
    for(int a=0;a<5;a++)for(int b=0;b<5;b++) {
        int64_t bx=total_x,by=total_y;frame(edge[a],edge[b],1);
        assert(total_x-bx==-edge[b]&&total_y-by==-edge[a]);cases++;
    }
    uint32_t seed=0x361035;
    for(int i=0;i<10000;i++) {
        seed=seed*1664525u+1013904223u;int x=(int)(seed&4095)-2048;
        seed=seed*1664525u+1013904223u;int y=(int)(seed&4095)-2048;
        int64_t bx=total_x,by=total_y;frame(x,y,1+(i%25));
        assert(total_x-bx==-y&&total_y-by==-x);cases++;
    }
    int r=reports;for(int i=0;i<100;i++)frame(0,0,1);assert(reports==r);cases+=100;
    shutter_value=46;frame(0,0,1);assert(pd.sw_smart_flag&&smart_writes==1&&last_smart_value==0x80);
    shutter_value=45;frame(0,0,1);assert(pd.sw_smart_flag&&smart_writes==1);
    shutter_value=44;frame(0,0,1);assert(!pd.sw_smart_flag&&smart_writes==2&&last_smart_value==0);
    assert(reports==r);shutter_value=0;
    for(int button=0;button<3;button++) {
        struct input_event btn={&sensor,INPUT_EV_KEY,INPUT_BTN_0+button,1,true};
        input_handler(&lc,&ld,&btn);assert(buttons==BIT(button));
        frame(5,-8,1);assert(buttons==BIT(button));
        btn.value=0;input_handler(&lc,&ld,&btn);assert(buttons==0);
    }
    read_error=-EIO;event_count=0;assert(pmw3610_report_data(&sensor)==-EIO&&event_count==0);
    read_error=0;pd.ready=false;assert(pmw3610_report_data(&sensor)==-EBUSY);
    printf("PASS: %d motion vectors; signed 12-bit limits, swap/inversion, split ABI, diagonal sync, no tail drift, 3-button drag/release, smart threshold, SPI failure/uninitialized guard\n",cases);
    return 0;
}

#!/usr/bin/env python3
"""Run actual pinned LVGL/Zephyr row rounding and vendored widget getters.

Default: host GCC/Clang. Windows without a host C compiler: --arm-gcc <path>
uses the Zephyr ARM compiler and optional unicorn==2.1.4 + pyelftools.
Only display metadata, event casts and current-state APIs are test doubles.
This is not USB, I2C, BLE or physical-display validation.
"""
import argparse
from pathlib import Path
import subprocess
import tempfile


def function(source, signature):
    start = source.index(signature)
    brace = source.index('{', start)
    depth = 1
    end = brace + 1
    while depth:
        depth += (source[end] == '{') - (source[end] == '}')
        end += 1
    return source[start:end]


p = argparse.ArgumentParser(description=__doc__)
p.add_argument('--workspace', type=Path, required=True)
p.add_argument('--cc', default='cc')
p.add_argument('--arm-gcc', type=Path)
a = p.parse_args()
repo = Path(__file__).resolve().parents[1]
ws = a.workspace.resolve()
widgets = repo / 'boards/shields/nano35_status/widgets'
lvgl = ws / 'modules/lib/gui/lvgl/src/core/lv_refr.c'
mono = ws / 'zephyr/modules/lvgl/lvgl_display_mono.c'
parts = [r'''
#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
typedef int16_t lv_coord_t;
typedef struct { lv_coord_t x1, y1, x2, y2; } lv_area_t;
struct display_capabilities { uint32_t screen_info; uint16_t x_resolution; };
struct lvgl_disp_data { struct display_capabilities cap; };
struct draw_buffer { uint32_t size; };
typedef struct lv_disp_drv_t {
    void *user_data;
    struct draw_buffer *draw_buf;
    void (*rounder_cb)(struct lv_disp_drv_t *, lv_area_t *);
} lv_disp_drv_t;
typedef struct { lv_disp_drv_t *driver; } lv_disp_t;
#define SCREEN_INFO_X_ALIGNMENT_WIDTH 1
#define SCREEN_INFO_MONO_VTILED 2
#define LV_LOG_WARN(...) ((void)0)
static lv_disp_t *disp_refr;
static int lv_area_get_height(const lv_area_t *a) { return a->y2 - a->y1 + 1; }
typedef struct { int type; uint8_t state; uint8_t indicators; } zmk_event_t;
struct zmk_wpm_state_changed { int type; uint8_t state; };
struct zmk_hid_indicators_changed { int type; uint8_t state; uint8_t indicators; };
static struct zmk_wpm_state_changed *as_zmk_wpm_state_changed(const zmk_event_t *e) {
    return e && e->type == 1 ? (void *)e : NULL;
}
static struct zmk_hid_indicators_changed *as_zmk_hid_indicators_changed(const zmk_event_t *e) {
    return e && e->type == 2 ? (void *)e : NULL;
}
static int zmk_wpm_get_state(void) { return 17; }
static uint8_t zmk_hid_indicators_get_current_profile(void) { return 5; }
struct bongo_cat_wpm_status_state { uint8_t wpm; };
struct hid_indicators_state { uint8_t hid_indicators; };
struct wpm_status_state { int wpm; const char *layer; };
static uint8_t zmk_keymap_highest_layer_active(void) { return 0; }
static const char *zmk_keymap_layer_name(uint8_t index) { return NULL; }

// SH1106 driver command/data transport double, backed by its 132-column RAM.
struct device { const void *config; };
struct ssd1306_config { uint8_t segment_offset; };
struct display_buffer_descriptor { uint16_t width, height; };
#define SSD1306_SET_LOWER_COL_ADDRESS 0x00
#define SSD1306_SET_HIGHER_COL_ADDRESS 0x10
#define SSD1306_SET_LOWER_COL_ADDRESS_MASK 0x0f
#define SSD1306_SET_PAGE_START_ADDRESS 0xb0
#define LOG_HEXDUMP_DBG(...) ((void)0)
#define LOG_ERR(...) ((void)0)
static uint8_t panel_ram[8][132];
static unsigned panel_col, panel_page, panel_bytes;
static int ssd1306_write_bus(const struct device *dev, const uint8_t *buf, size_t len, bool command) {
    if (command) {
        if (len != 3 || (buf[1] & 0xf0) != 0x10 || (buf[2] & 0xf8) != 0xb0) return -1;
        panel_col = (buf[0] & 15) | ((buf[1] & 15) << 4); panel_page = buf[2] & 7;
    } else {
        if (panel_col + len > 132 || panel_page > 7) return -1;
        for (size_t i=0; i<len; i++) panel_ram[panel_page][panel_col++] = buf[i];
        panel_bytes += len;
    }
    return 0;
}
''']
parts.append(function((ws/'zephyr/drivers/display/ssd1306.c').read_text(), 'static int ssd1306_write_sh1106('))
parts.append(function(mono.read_text(), 'void lvgl_rounder_cb_mono('))
parts.append(function(lvgl.read_text(), 'static uint32_t get_max_row(lv_disp_t * disp, lv_coord_t area_w, lv_coord_t area_h)\n{'))
parts.append(function((widgets / 'bongo_cat.c').read_text(), 'struct bongo_cat_wpm_status_state bongo_cat_wpm_status_get_state('))
parts.append(function((widgets / 'hid_indicators.c').read_text(), 'static struct hid_indicators_state hid_indicators_get_state('))
parts.append(function((widgets / 'wpm_status.c').read_text(), 'static struct wpm_status_state get_state('))
parts.append(function((widgets / 'wpm_status.c').read_text(), 'static uint16_t cpm_from_wpm('))
parts.append(r'''
#define ZMK_SPLIT_BLE_PERIPHERAL_COUNT 2
#define LED_NLCK 1
#define LED_CLCK 2
#define LED_SLCK 4
static int side_by_source[2];
static void atomic_set(int *p, int value) { *p=value; }
static int equal(const char *a,const char *b) {
    while (*a && *a==*b) { a++; b++; } return *a==*b;
}
''')
for signature in ['static char side_for_position(', 'static void battery_text(', 'static void learn_battery_side(']:
    parts.append(function((widgets/'battery_status.c').read_text(), signature))
parts.append(function((widgets/'hid_indicators.c').read_text(), 'static void lock_text('))
parts.append(r'''
int main(void) {
    struct lvgl_disp_data data = { .cap = { .screen_info = SCREEN_INFO_MONO_VTILED, .x_resolution = 129 } };
    /* Original defaults: 129x64, 10%, 32 static bits per pixel => 825 pixels. */
    struct draw_buffer buffer = { .size = (10 * 129 * 64) / 100 };
    lv_disp_drv_t driver = { .user_data = &data, .draw_buf = &buffer, .rounder_cb = lvgl_rounder_cb_mono };
    lv_disp_t display = { .driver = &driver };
    disp_refr = &display;
    if (get_max_row(&display, 129, 64) != 0) return 1;
    /* A zero increment in LVGL's row += max_row loop never makes progress. */
    data.cap.x_resolution = 128;
    buffer.size = 128 * 64;
    if (get_max_row(&display, 128, 64) != 64) return 2;
    for (int w = 1; w <= 128; w++) {
        for (int h = 8; h <= 64; h += 8) {
            uint32_t rows = get_max_row(&display, w, h);
            if (rows == 0 || rows % 8 || rows > h || rows * w > buffer.size) return 3;
        }
    }
    if (bongo_cat_wpm_status_get_state(NULL).wpm != 17) return 4;
    zmk_event_t wpm = { .type = 1, .state = 82 };
    if (bongo_cat_wpm_status_get_state(&wpm).wpm != 82) return 5;
    if (hid_indicators_get_state(NULL).hid_indicators != 5) return 6;
    zmk_event_t hid = { .type = 2, .indicators = 2 };
    if (hid_indicators_get_state(&hid).hid_indicators != 2) return 7;
    zmk_event_t unrelated = { .type = 3 };
    if (bongo_cat_wpm_status_get_state(&unrelated).wpm != 17) return 8;
    if (hid_indicators_get_state(&unrelated).hid_indicators != 5) return 9;
    // A standard centered 128-column panel exposes RAM columns 2..129.
    // In reverse-display mode 0xff is blank; old column 129 retains four lit dots.
    static uint8_t blank[1024];
    for (unsigned i=0; i<sizeof(blank); i++) blank[i]=0xff;
    for (unsigned page=0; page<8; page++) for (unsigned col=0; col<132; col++) panel_ram[page][col]=0xff;
    panel_ram[0][129]=0xfe; panel_ram[2][129]=0xfb; panel_ram[5][129]=0xef; panel_ram[7][129]=0x7f;
    struct ssd1306_config cfg={.segment_offset=1}; struct device dev={.config=&cfg};
    struct display_buffer_descriptor desc={.width=128, .height=64};
    if (ssd1306_write_sh1106(&dev,0,0,&desc,blank,sizeof(blank))) return 10;
    if (panel_ram[0][129]!=0xfe || panel_ram[2][129]!=0xfb || panel_ram[5][129]!=0xef || panel_ram[7][129]!=0x7f) return 11;
    cfg.segment_offset=2; panel_bytes=0;
    if (ssd1306_write_sh1106(&dev,0,0,&desc,blank,sizeof(blank))) return 12;
    if (panel_bytes!=1024) return 13;
    for (unsigned page=0; page<8; page++) for (unsigned col=2; col<=129; col++) {
        if (panel_ram[page][col]!=0xff) return 14;
    }
    // A partial refresh at logical x=127 must also reach the last visible column.
    desc.width=1; desc.height=8; blank[0]=0x00;
    if (ssd1306_write_sh1106(&dev,127,56,&desc,blank,1) || panel_ram[7][129]!=0x00 || panel_ram[7][128]!=0xff) return 15;
    // New speed widget must initialize without an event or a layer display-name.
    if (get_state(NULL).wpm!=17 || get_state(NULL).layer!=NULL) return 16;
    if (get_state(&wpm).wpm!=82 || get_state(&unrelated).wpm!=17) return 17;
    if (cpm_from_wpm(0)!=0 || cpm_from_wpm(82)!=410 || cpm_from_wpm(255)!=1275) return 18;
    if (cpm_from_wpm(-1)!=0 || cpm_from_wpm(999)!=1275) return 19;
    char text[8];
    battery_text(text,sizeof(text),0,59);if(!equal(text,"? 59%")) return 20;
    battery_text(text,sizeof(text),'L',100);if(!equal(text,"L100%")) return 21;
    battery_text(text,sizeof(text),'R',7);if(!equal(text,"R  7%")) return 22;
    battery_text(text,sizeof(text),'R',255);if(!equal(text,"R100%")) return 23;
    for(unsigned pos=0;pos<35;pos++) {
        char expected=pos<30?(pos%10<5?'L':'R'):(pos<33?'L':'R');
        if(side_for_position(pos)!=expected) return 24;
    }
    learn_battery_side(0,16);if(side_by_source[0]!='R' || side_by_source[1]!='L') return 25;
    learn_battery_side(255,11);learn_battery_side(0,35);
    if(side_by_source[0]!='R' || side_by_source[1]!='L') return 26;
    learn_battery_side(0,30);if(side_by_source[0]!='L' || side_by_source[1]!='R') return 27;
    const char *expected_locks[]={"","N","C","NC","S","NS","CS","NCS"};
    for(int i=0;i<8;i++) {
        lock_text(text,i|0x18);if(!equal(text,expected_locks[i])) return 28;
    }
    return 0;
}
''')
with tempfile.TemporaryDirectory(prefix='display-test-') as tmp:
    src = Path(tmp) / 'display.c'
    src.write_text('\n'.join(parts))
    binary = Path(tmp) / ('display.elf' if a.arm_gcc else 'display.exe')
    if a.arm_gcc:
        subprocess.run([str(a.arm_gcc), '-std=c99', '-O0', '-ffreestanding', '-fno-builtin',
                        '-nostdlib', '-mcpu=cortex-m4', '-mthumb', '-Wl,-Ttext=0x10000,-e,main',
                        str(src), '-o', str(binary)], check=True)
        from elftools.elf.elffile import ELFFile
        from unicorn import Uc, UC_ARCH_ARM, UC_MODE_THUMB, UC_MODE_MCLASS
        from unicorn.arm_const import UC_ARM_REG_SP, UC_ARM_REG_LR, UC_ARM_REG_R0
        emu = Uc(UC_ARCH_ARM, UC_MODE_THUMB | UC_MODE_MCLASS)
        emu.mem_map(0x20000000, 0x10000)
        emu.mem_map(0x30000000, 0x1000)
        with binary.open('rb') as f:
            elf = ELFFile(f)
            segments = [seg for seg in elf.iter_segments() if seg['p_type'] == 'PT_LOAD']
            first = min(seg['p_vaddr'] for seg in segments) & ~0xfff
            last = max(seg['p_vaddr'] + seg['p_memsz'] for seg in segments)
            emu.mem_map(first, (last - first + 0xfff) & ~0xfff)
            for seg in segments:
                if seg['p_filesz']:
                    emu.mem_write(seg['p_vaddr'], seg.data())
            entry = elf.header['e_entry']
        emu.reg_write(UC_ARM_REG_SP, 0x2000fff0)
        emu.reg_write(UC_ARM_REG_LR, 0x30000001)
        emu.emu_start(entry | 1, 0x30000000, count=1000000)
        from unicorn.arm_const import UC_ARM_REG_PC
        assert emu.reg_read(UC_ARM_REG_PC) == 0x30000000, 'test did not return'
        assert emu.reg_read(UC_ARM_REG_R0) == 0, f'C test failed: {emu.reg_read(UC_ARM_REG_R0)}'
    else:
        subprocess.run([a.cc, '-std=c99', '-O0', '-fsanitize=undefined', '-fno-sanitize-recover=all',
                        str(src), '-o', str(binary)], check=True)
        subprocess.run([str(binary)], check=True)
print('PASS: actual LVGL/Zephyr C reproduces old zero-row refresh; 1024 corrected row cases advance')
print('PASS: actual bongo/HID getters handle NULL startup, real events and unrelated events')
print('PASS: actual SH1106 writer reproduces stale edge at offset=1 and refreshes all 128 visible columns at offset=2; full frame and x=127 partial update')
print('PASS: actual speed getter handles NULL/unrelated events and unnamed layers; CPM conversion is bounded to 0..1275')
print('LIMIT: metadata/event/bus APIs and panel RAM mapping are doubles; USB/I2C/BLE/hardware are not emulated')

print('PASS: battery label formatting, all 35 handed positions, reversed BLE sources, invalid sources and all eight active-only lock combinations')

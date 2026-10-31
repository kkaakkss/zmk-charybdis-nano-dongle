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
''']
parts.append(function(mono.read_text(), 'void lvgl_rounder_cb_mono('))
parts.append(function(lvgl.read_text(), 'static uint32_t get_max_row(lv_disp_t * disp, lv_coord_t area_w, lv_coord_t area_h)\n{'))
parts.append(function((widgets / 'bongo_cat.c').read_text(), 'struct bongo_cat_wpm_status_state bongo_cat_wpm_status_get_state('))
parts.append(function((widgets / 'hid_indicators.c').read_text(), 'static struct hid_indicators_state hid_indicators_get_state('))
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
        emu.emu_start(entry | 1, 0x30000000, count=200000)
        from unicorn.arm_const import UC_ARM_REG_PC
        assert emu.reg_read(UC_ARM_REG_PC) == 0x30000000, 'test did not return'
        assert emu.reg_read(UC_ARM_REG_R0) == 0, f'C test failed: {emu.reg_read(UC_ARM_REG_R0)}'
    else:
        subprocess.run([a.cc, '-std=c99', '-O0', '-fsanitize=undefined', '-fno-sanitize-recover=all',
                        str(src), '-o', str(binary)], check=True)
        subprocess.run([str(binary)], check=True)
print('PASS: actual LVGL/Zephyr C reproduces old zero-row refresh; 1024 corrected row cases advance')
print('PASS: actual bongo/HID getters handle NULL startup, real events and unrelated events')
print('LIMIT: metadata/event APIs are doubles; USB/I2C/BLE/hardware are not emulated')

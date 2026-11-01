#!/usr/bin/env python3
"""Actual Pomodoro behavior/model/widget C with deterministic clock/queue/LVGL doubles.

Host GCC/Clang uses UBSan; ARM uses Unicorn. This does not emulate the real
display driver, allocator, kernel concurrency, BLE or USB hardware.
"""
from pathlib import Path
import argparse, re, subprocess, tempfile

p=argparse.ArgumentParser(description=__doc__)
cc=p.add_mutually_exclusive_group(required=True)
cc.add_argument('--arm-gcc',type=Path)
cc.add_argument('--host-cc')
a=p.parse_args()
repo=Path(__file__).resolve().parents[1]
engine=(repo/'src/pomodoro.c').read_text(encoding='utf-8')
widget=(repo/'boards/shields/nano35_status/widgets/pomodoro.c').read_text(encoding='utf-8')
engine=re.sub(r'__attribute__\(\(weak\)\) void nano35_pomodoro_ui_changed\(bool notify\) \{\}', '', engine)
def without_includes(s):
    return re.sub(r'^#include[^\n]*\n','',s,flags=re.M)
source='\n'.join([(repo/'tests/pomodoro_stubs.h').read_text(encoding='utf-8'),
                   without_includes(engine),without_includes(widget),
                   (repo/'tests/pomodoro_harness.c').read_text(encoding='utf-8')])
with tempfile.TemporaryDirectory(prefix='nano-pomodoro-') as tmp:
    tmp=Path(tmp); c=tmp/'test.c'; elfpath=tmp/'test.elf'
    c.write_text(source,encoding='utf-8')
    if a.host_cc:
        exe=tmp/'test-native'
        subprocess.run([a.host_cc,'-std=c11','-O1','-g','-fno-builtin','-fsanitize=undefined',
                        '-fno-sanitize-recover=all','-I',str(repo/'src'),str(c),'-o',str(exe)],check=True)
        subprocess.run([str(exe)],check=True)
    else:
        subprocess.run([str(a.arm_gcc),'-std=c11','-O0','-DPOMO_ARM_TEST','-ffreestanding',
                        '-fno-builtin','-nostdlib','-mcpu=cortex-m4','-mthumb',
                        '-Wl,-Ttext=0x10000,-e,main','-I',str(repo/'src'),str(c),'-lgcc','-o',str(elfpath)],check=True)
        from elftools.elf.elffile import ELFFile
        from unicorn import Uc,UC_ARCH_ARM,UC_MODE_THUMB,UC_MODE_MCLASS
        from unicorn.arm_const import UC_ARM_REG_SP,UC_ARM_REG_LR,UC_ARM_REG_R0,UC_ARM_REG_PC
        emu=Uc(UC_ARCH_ARM,UC_MODE_THUMB|UC_MODE_MCLASS)
        emu.mem_map(0x20000000,0x10000);emu.mem_map(0x30000000,0x1000)
        with elfpath.open('rb') as f:
            elf=ELFFile(f);segments=[s for s in elf.iter_segments() if s['p_type']=='PT_LOAD']
            first=min(s['p_vaddr'] for s in segments)&~0xfff
            last=max(s['p_vaddr']+s['p_memsz'] for s in segments)
            emu.mem_map(first,(last-first+0xfff)&~0xfff)
            for seg in segments:
                if seg['p_filesz']:emu.mem_write(seg['p_vaddr'],seg.data())
            entry=elf.header['e_entry']
        emu.reg_write(UC_ARM_REG_SP,0x2000fff0);emu.reg_write(UC_ARM_REG_LR,0x30000001)
        emu.emu_start(entry|1,0x30000000,count=20000000)
        assert emu.reg_read(UC_ARM_REG_PC)==0x30000000,'test did not finish'
        assert emu.reg_read(UC_ARM_REG_R0)==0,f'Pomodoro failed case {emu.reg_read(UC_ARM_REG_R0)}'
print('PASS: 4 x 25min focus + 3 x 5min break, exact deadlines, 6900 ticks, delayed catch-up, pause/resume/reset/restart, 64-bit uptime')
print('PASS: actual behavior + widget C; queued LVGL only, selected layout bounds, phase labels, idle wake/reblank, idle race, reset cancels alert, no key/activity fabrication')
print('LIMIT: clock/kernel/queues/LVGL/display are doubles; real allocation, timing/concurrency and hardware remain unverified')
print('Instrumentation: host UBSan' if a.host_cc else 'Instrumentation: ARM emulation without UBSan')

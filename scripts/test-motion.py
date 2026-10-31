#!/usr/bin/env python3
"""Compile actual pinned C functions with fake hardware/transport, under UBSan."""
import argparse, re, subprocess, tempfile
from pathlib import Path
p=argparse.ArgumentParser();p.add_argument('--workspace',type=Path,required=True);a=p.parse_args()
repo=Path(__file__).resolve().parents[1];ws=a.workspace.resolve()
def function(s,signature):
    start=s.index(signature);body=s.index('{',start);depth=1;i=body+1
    while depth:
        if s[i]=='{':depth+=1
        elif s[i]=='}':depth-=1
        i+=1
    return s[start:i]+'\n'
driver=(ws/'zmk-pmw3610-driver/src/pmw3610.c').read_text()
header=(ws/'zmk-pmw3610-driver/src/pmw3610.h').read_text()
listener=(ws/'zmk/app/src/pointing/input_listener.c').read_text()
wire=(ws/'zmk/app/include/zmk/split/bluetooth/service.h').read_text()
conf=(repo/'boards/shields/nano35/nano35_right.conf').read_text()
assert re.search(r'^CONFIG_PMW3610_REPORT_INTERVAL_MIN=0$',conf,re.M), 'Lossy report limiter must stay disabled'
for flag in ['SWAP_XY','INVERT_X','INVERT_Y']:
    assert re.search(r'^CONFIG_PMW3610_'+flag+r'=y$',conf,re.M), 'Harness orientation must match the firmware'
with tempfile.TemporaryDirectory(prefix='nano35-motion-') as t:
    t=Path(t)
    constants='\n'.join(x for x in header.splitlines() if re.match(r'#define PMW3610_(BURST_SIZE|X_L_POS|Y_L_POS|XY_H_POS|SHUTTER_H_POS|SHUTTER_L_POS|REG_MOTION_BURST) ',x))
    (t/'driver_report.inc').write_text(constants+'\n'+function(driver,'static int pmw3610_report_data('))
    (t/'listener_types.inc').write_text(listener[listener.index('enum input_listener_xy_data_mode'):listener.index('static void handle_rel_code')])
    signatures=['static void handle_rel_code(', 'static void handle_abs_code(', 'static void handle_key_code(', 'static void clear_xy_data(', 'static void input_handler(']
    (t/'listener_functions.inc').write_text('\n'.join(function(listener,s) for s in signatures))
    start=wire.index('struct zmk_split_input_event_payload {');end=wire.index('} __packed;',start)+len('} __packed;')
    (t/'wire_struct.inc').write_text(wire[start:end].replace('__packed','__attribute__((packed))'))
    for interval in [12,0]:
        exe=t/f'motion-{interval}'
        cmd=['gcc','-std=c11','-O1','-g','-Wall','-Wextra','-Werror','-Wno-unused-parameter','-Wno-unused-function','-fsanitize=undefined','-fno-sanitize-recover=all',f'-DCONFIG_PMW3610_REPORT_INTERVAL_MIN={interval}','-I',str(t),str(repo/'tests/motion_harness.c'),'-o',str(exe)]
        subprocess.run(cmd,check=True)
        subprocess.run([str(exe)]+(['legacy'] if interval else []),check=True)

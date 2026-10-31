#!/usr/bin/env python3
"""Validate actual generated DTS/Kconfig and UF2, not only source syntax."""
import argparse
import pickle
import struct
import sys
from pathlib import Path
import yaml
p = argparse.ArgumentParser()
p.add_argument('--workspace', type=Path, required=True)
a = p.parse_args()
ws = a.workspace.resolve()
sys.path.insert(0, str(ws / 'zephyr/scripts/dts/python-devicetree/src'))
repo = Path(__file__).resolve().parents[1]
for i,t in enumerate(yaml.safe_load((repo/'build.yaml').read_text())['include']):
    d=ws/'builds'/str(i)/'zephyr'
    config=dict(line.split('=',1) for line in (d/'.config').read_text().splitlines() if line.startswith('CONFIG_') and '=' in line)
    b=(d/'zmk.uf2').read_bytes()
    assert len(b)>0 and len(b)%512==0
    for off in range(0,len(b),512):
        h=struct.unpack_from('<8I',b,off)
        assert h[:2]==(0x0a324655,0x9e5d5157)
        assert h[7]==0xada52840 and 0x26000<=h[3]<0xec000
        assert struct.unpack_from('<I',b,off+508)[0]==0x0ab16f30
    if t['shield']=='settings_reset':
        assert config['CONFIG_ZMK_SETTINGS_RESET_ON_START']=='y'
    else:
        edt=pickle.load((d/'edt.pickle').open('rb'))
        layout=edt.chosen_node('zmk,physical-layout')
        coords=layout.props['transform'].val.props['map'].val
        assert len(coords)==len(set(coords))==35
        assert all(v>>8<4 and v&255<10 for v in coords)
        assert len(layout.props['keys'].val)==35
        for layer in ('default_layer', 'f_grave_navi_bt_layer', 'navi_num', 'osl_layer', 'mouse_layer', 'scroll_layer', 'snipe_layer'):
            assert len(edt.get_node('/keymap/'+layer).props['bindings'].val)==35
        assert config['CONFIG_ZMK_POINTING']=='y'
        assert config['CONFIG_ZMK_SPLIT_BLE']=='y'
        split=edt.get_node('/split_inputs/trackball_split@0')
        assert split.regs[0].addr==0
        if t['shield'].split()[0]=='nano35_dongle':
            assert config['CONFIG_ZMK_INPUT_SPLIT']=='y'
            assert config['CONFIG_ZMK_SPLIT_BLE_CENTRAL_POSITION_QUEUE_SIZE']=='64'
            assert config['CONFIG_INPUT_QUEUE_MAX_MSGS']=='64'
            assert config['CONFIG_ZMK_SPLIT_ROLE_CENTRAL']=='y'
            assert config['CONFIG_ZMK_SPLIT_BLE_CENTRAL_PERIPHERALS']=='2'
            assert config['CONFIG_BT_MAX_CONN']=='3' and config['CONFIG_BT_MAX_PAIRED']=='5'
            assert config['CONFIG_ZMK_USB']=='y'
            assert edt.get_node('/trackball_listener').props['device'].val==split
            processor=edt.get_node('/nano35_ball_modes')
            assert edt.get_node('/trackball_listener').props['input-processors'].val[0].controller==processor
            assert processor.props['mouse-layer'].val==4
            assert processor.props['scroll-layer'].val==5
            assert processor.props['snipe-layer'].val==6
            assert processor.props['automouse-timeout-ms'].val==30000
            assert config['CONFIG_ZMK_INPUT_PROCESSOR_TEMP_LAYER']=='y'
            if t.get('snippet')=='nano35-oled':
                oled=edt.chosen_node('zephyr,display')
                assert oled.matching_compat=='sinowealth,sh1106'
                assert oled.regs[0].addr==0x3c
                assert oled.props['width'].val==128 and oled.props['height'].val==64
                assert oled.props['segment-offset'].val==1
                assert config['CONFIG_ZMK_DISPLAY_STATUS_SCREEN_CUSTOM']=='y'
                assert config['CONFIG_ZMK_DISPLAY_WORK_QUEUE_DEDICATED']=='y'
                assert config['CONFIG_ZMK_DISPLAY_DEDICATED_THREAD_STACK_SIZE']=='4096'
                assert config['CONFIG_LV_COLOR_DEPTH_1']=='y'
                assert config['CONFIG_LV_Z_BITS_PER_PIXEL']=='1'
                assert config['CONFIG_LV_Z_VDB_SIZE']=='100'
                assert config['CONFIG_ZMK_DONGLE_DISPLAY_BONGO_CAT']=='y'
                assert config['CONFIG_ZMK_DONGLE_DISPLAY_LAYER']=='y'
                for layer, label in [('default_layer', 'QWERTY'), ('f_grave_navi_bt_layer', 'NAV'), ('navi_num', 'NUM'), ('osl_layer', 'OSL'), ('mouse_layer', 'MOUSE'), ('scroll_layer', 'SCROLL'), ('snipe_layer', 'SNIPE')]:
                    assert edt.get_node('/keymap/'+layer).props['display-name'].val==label
            if t.get('snippet')=='studio-rpc-usb-uart':
                assert config['CONFIG_ZMK_STUDIO']=='y'
                assert edt.get_node('/combos/nano35_studio_unlock').props['bindings'].val[0].controller.name=='studio_unlock'
        else:
            assert config.get('CONFIG_ZMK_SPLIT_ROLE_CENTRAL')!='y'
            scan=edt.chosen_node('zmk,kscan')
            assert [g.data['pin'] for g in scan.props['row-gpios'].val]==[31,15,24,22]
            assert [g.data['pin'] for g in scan.props['col-gpios'].val]==[2,29,9,0,11]
            assert [g.controller.name for g in scan.props['row-gpios'].val]==['gpio@50000000','gpio@50000300','gpio@50000000','gpio@50000000']
            assert [g.controller.name for g in scan.props['col-gpios'].val]==['gpio@50000000','gpio@50000000','gpio@50000000','gpio@50000300','gpio@50000000']
            if t['shield']=='nano35_right':
                assert config['CONFIG_ZMK_INPUT_SPLIT']=='y'
                assert config['CONFIG_PMW3610_REPORT_INTERVAL_MIN']=='0'
                assert config['CONFIG_PMW3610_SMART_ALGORITHM']=='y'
                assert config['CONFIG_INPUT_QUEUE_MAX_MSGS']=='64'
                assert config['CONFIG_BT_L2CAP_TX_BUF_COUNT']==config['CONFIG_BT_CONN_TX_MAX']==config['CONFIG_BT_BUF_ACL_TX_COUNT']=='8'
                assert config['CONFIG_PMW3610']=='y'
                assert layout.props['transform'].val.props['col-offset'].val==5
                assert split.props['device'].val.matching_compat=='pixart,pmw3610'
                assert split.props['device'].val.props['irq-gpios'].val[0].data['pin']==6
            else:
                assert split.status=='disabled'
    print(f'PASS {i}: {t["artifact-name"]} compiled DTS/Kconfig and {len(b)//512} UF2 blocks')

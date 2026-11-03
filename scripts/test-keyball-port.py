#!/usr/bin/env python3
"""Compare the three mouse layers against the unmodified Keyball39 source."""
from pathlib import Path
import argparse,re,pickle,sys
p=argparse.ArgumentParser()
p.add_argument('--keyball-keymap',type=Path,required=True)
p.add_argument('--workspace',type=Path)
a=p.parse_args()
repo=Path(__file__).resolve().parents[1]
ref=a.keyball_keymap.read_text(encoding='utf-8-sig')
new=(repo/'config/nano35.keymap').read_text(encoding='utf-8-sig')
def body(text,name):
    m=re.search(r'\b'+name+r'\s*\{',text);assert m,name
    start=m.end(); end=start; depth=1
    while depth:
        depth+=(text[end]=='{')-(text[end]=='}');end+=1
    return text[start:end-1]
def entries(text,name):
    s=re.search(r'bindings\s*=\s*<(.*?)>;',body(text,name),re.S)[1]
    return [' '.join(x.split()) for x in re.findall(r'&[^&<>]+',s)]
keep=list(range(30))+[33,34,35,36,37]
for name in ['mouse_layer','scroll_layer','snipe_layer']:
    old=entries(ref,name); actual=entries(new,name)
    assert len(old)==39 and len(actual)==35
    expected=[old[i] for i in keep]
    if name=='mouse_layer':
        for index,binding in {1:'&ball_speed 1',11:'&kp LSHIFT',21:'&ball_speed 0',32:'&to 0'}.items():
            expected[index]=binding
    elif name=='scroll_layer':
        expected[1]='&ball_speed 3'; expected[11]='&ball_speed 2'
    elif name=='snipe_layer':
        expected[1]='&ball_speed 5'; expected[11]='&ball_speed 4'
    assert actual==expected,name
for oldname,newname in [('lclick_to0','lclick_to0'),('alt_click','alt_click'),('alt_right_click','alt_right_click')]:
    # DTS phandle-array group boundaries do not change the binding sequence.
    expected_macro=re.sub(r'>\s*,\s*<',' ',body(ref,oldname))
    actual_macro=re.sub(r'>\s*,\s*<',' ',body(new,newname))
    assert ' '.join(expected_macro.split())==' '.join(actual_macro.split())
print('PASS: all 105 Mouse/Scroll/Snipe bindings with explicit per-mode user overrides and 3 mouse macros match mapped Keyball39 source')
if a.workspace:
    ws=a.workspace.resolve();sys.path.insert(0,str(ws/'zephyr/scripts/dts/python-devicetree/src'))
    for i in [0,4,5]:
        edt=pickle.load((ws/f'builds/{i}/zephyr/edt.pickle').open('rb'))
        mouse=edt.get_node('/keymap/mouse_layer').props['bindings'].val
        assert mouse[2].controller.name=='momentary_layer' and mouse[2].data['param1']==5
        assert mouse[3].controller.name=='momentary_layer' and mouse[3].data['param1']==6
        for pos in [12,17]: assert mouse[pos].controller.name=='mouse_key_press' and mouse[pos].data['param1']==1
        for pos in [13,18]: assert mouse[pos].controller.name=='mouse_key_press' and mouse[pos].data['param1']==2
        middle=edt.get_node('/combos/nano35_mouse_middle')
        assert middle.props['key-positions'].val==[17,18]
        assert middle.props['layers'].val==[4,5,6]
        assert middle.props['bindings'].val[0].data['param1']==4
        assert edt.get_node('/combos/nano35_mouse_manual').props['bindings'].val[0].data['param1']==4
        for name in ['kp','bra_R','BKT_L','BKT_R']:
            assert edt.get_node('/combos/'+name).props['layers'].val==[0,1,2,3]
        for pos in [33,34]:
            assert mouse[pos].controller.name=='to_layer' and mouse[pos].data['param1']==0
        assert edt.get_node('/msc_input_listener').props.get('input-processors') is None
    print('PASS: actual central DTS mode holds, L/R/middle click, exit thumbs, keyboard-combo isolation, separate key-wheel listener')
print('LIMIT: real key timing, button feel, scroll direction/speed and USB/BLE need device testing')

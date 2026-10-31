#!/usr/bin/env python3
"""Check the TOTEM->Nano35 port against the untouched reference keymap."""
import argparse
from pathlib import Path
import re, pickle, sys

p=argparse.ArgumentParser()
p.add_argument('--totem-keymap',type=Path,required=True)
p.add_argument('--workspace',type=Path)
a=p.parse_args()
repo=Path(__file__).resolve().parents[1]
original=a.totem_keymap.read_text(encoding='utf-8-sig')
ported=(repo/'config/nano35.keymap').read_text(encoding='utf-8-sig')
keep=list(range(20))+list(range(21,31))+list(range(32,37))
assert len(keep)==35
index={old:new for new,old in enumerate(keep)}

def block(text,name):
    m=re.search(r'\b'+re.escape(name)+r'\s*\{',text)
    assert m,name
    start=m.end(); end=start; depth=1
    while depth:
        depth+=(text[end]=='{')-(text[end]=='}'); end+=1
    return text[start:end-1]

def norm(text):
    return ' '.join(text.split())

def reindex(text):
    def replace(m):
        values=[int(v) for v in m[2].split()]
        return m[1]+' = <'+' '.join(str(index[v]) for v in values if v in index)+'>;'
    return re.sub(r'(key-positions|hold-trigger-key-positions)\s*=\s*<([\d\s]+)>;',replace,text)

layers=['default_layer','f_grave_navi_bt_layer','navi_num','osl_layer']
for name in layers:
    old=re.search(r'bindings\s*=\s*<(.*?)>;',block(original,name),re.S)[1]
    new=re.search(r'bindings\s*=\s*<(.*?)>;',block(ported,name),re.S)[1]
    old=[norm(s) for s in re.findall(r'&[^&<>]+',old)]
    new=[norm(s) for s in re.findall(r'&[^&<>]+',new)]
    assert len(old)==38 and len(new)==35
    assert new==[old[i] for i in keep],name
print('PASS: all 140 layer bindings equal TOTEM after removing only positions 20/31/37')
assert norm(block(ported,'behaviors'))==norm(reindex(block(original,'behaviors')))
for macro in ['Hiragana', 'bluetooth']:
    assert norm(block(ported,macro))==norm(block(original,macro))
for name in ['lt','mt','caps_word']:
    assert norm(block(ported,name))==norm(block(original,name))
print('PASS: custom behaviors, timing, macros and remapped home-row hold triggers preserved')
combos=block(original,'combos')
names=re.findall(r'^\s{8}([A-Za-z_]\w*)\s*\{',combos,re.M)
for name in names:
    expected=reindex(block(combos,name))
    if 'layers =' not in expected:
        expected += ' layers = <0 1 2 3>;'
    assert norm(block(ported,name))==norm(expected),name
print(f'PASS: all {len(names)} original combos preserve actions/layers and remap positions')
if a.workspace:
    ws=a.workspace.resolve()
    sys.path.insert(0,str(ws/'zephyr/scripts/dts/python-devicetree/src'))
    for i in [0,1,2,4,5]:
        d=ws/f'builds/{i}/zephyr'
        edt=pickle.load((d/'edt.pickle').open('rb'))
        for name in layers:
            assert len(edt.get_node('/keymap/'+name).props['bindings'].val)==35
        for name in names:
            expected=re.search(r'key-positions\s*=\s*<([\d\s]+)>;',reindex(block(combos,name)))[1]
            assert edt.get_node('/combos/'+name).props['key-positions'].val==[int(v) for v in expected.split()]
        for name in ['homerow_left','homerow_right']:
            expected=re.search(r'hold-trigger-key-positions\s*=\s*<([\d\s]+)>;',reindex(block(original,name)))[1]
            assert edt.get_node('/behaviors/'+name).props['hold-trigger-key-positions'].val==[int(v) for v in expected.split()]
        cache=(ws/f'builds/{i}/CMakeCache.txt').read_text(encoding='utf-8')
        assert 'KEYMAP_FILE:UNINITIALIZED='+str(ws/'nano35-source/config/nano35.keymap').replace('\\','/') in cache
    print('PASS: all 5 keyboard builds contain 4x35 bindings, mapped combos/hold triggers and the explicit Nano source path')
print('LIMIT: this checks source/generated mappings; physical key positions and tap/hold feel need device testing')

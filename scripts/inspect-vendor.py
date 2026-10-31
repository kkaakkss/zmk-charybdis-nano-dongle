#!/usr/bin/env python3
"""Check the specific seller ZIP; decode GPIO arrays and transform without executing firmware."""
import argparse
import hashlib
import json
import re
import struct
import zipfile
from pathlib import Path
p=argparse.ArgumentParser()
p.add_argument('zip')
a=p.parse_args()
rows=[(0,31,0),(1,15,0),(0,24,0),(0,22,0)]
cols=[(0,2,32),(0,29,32),(0,9,32),(1,0,32),(0,11,32)]
lookup=list(range(1,6))+list(range(10,5,-1))+list(range(11,16))+list(range(20,15,-1))+list(range(21,26))+list(range(30,25,-1))+[0,0,31,32,33,0,0,0,35,34]
declared=re.findall(r'RC\((\d+),(\d+)\)', (Path(__file__).resolve().parents[1]/'boards/shields/nano35/nano35.dtsi').read_text())
compiled_lookup=[0]*40
for position,(row,col) in enumerate(declared):
    compiled_lookup[int(row)*10+int(col)]=position+1
assert compiled_lookup==lookup, 'Repository transform differs from seller firmware'
reports=[]
with zipfile.ZipFile(a.zip) as z:
    for name in z.namelist():
        if '/' in name or not name.endswith('.uf2'): continue
        uf2=z.read(name);n=len(uf2)//512;blocks={};base=0x26000
        assert len(uf2)%512==0
        for off in range(0,len(uf2),512):
            h=struct.unpack_from('<8I',uf2,off)
            assert h[:2]==(0x0a324655,0x9e5d5157) and h[7]==0xada52840
            assert h[4]==256 and h[6]==n and h[2]&0x2000
            assert struct.unpack_from('<I',uf2,off+508)[0]==0x0ab16f30
            assert h[5] not in blocks
            blocks[h[5]]=(h[3],uf2[off+32:off+288])
        assert sorted(blocks)==list(range(n))
        end=max(addr+len(b) for addr,b in blocks.values());data=bytearray(b'\xff'*(end-base))
        for addr,b in blocks.values(): data[addr-base:addr-base+len(b)]=b
        report={'file':name,'sha256':hashlib.sha256(uf2).hexdigest(),'uf2_blocks':n}
        if name.startswith('charybdis_'):
            ports={}
            for off in range(0,len(data)-24,4):
                ptr=struct.unpack_from('<I',data,off)[0]
                if base<=ptr<end:
                    label=bytes(data[ptr-base:ptr-base+25]).split(b'\0')[0]
                    if label in (b'gpio@50000000',b'gpio@50000300'):
                        ports[base+off]=int(label==b'gpio@50000300')
            assert set(ports.values())=={0,1}
            for label,expected in [('rows',rows),('cols',cols)]:
                matches=[]
                for off in range(0,len(data)-12*len(expected),4):
                    values=[struct.unpack_from('<IBBHI',data,off+i*12) for i in range(len(expected))]
                    if all(ptr in ports and (ports[ptr],pin,flags)==expected[i] and index==i for i,(ptr,pin,pad,flags,index) in enumerate(values)):
                        matches.append(hex(base+off))
                assert len(matches)==1,(name,label,matches)
                report[label+'_initializer_address']=matches[0]
            pos=data.find(struct.pack('<40I',*lookup));assert pos>=0
            offset=5 if 'right' in name else 0
            transform=struct.pack('<II4B',base+pos,40,4,10,offset,0)
            assert data.find(transform)>=0
            report['transform']={'lookup_address':hex(base+pos),'rows':4,'columns':10,'column_offset':offset,'keys':35}
        reports.append(report)
print(json.dumps(reports,indent=2))

#!/bin/bash
# strs.sh addr len : print printable ascii runs (>=4) with offsets
$HOME/cmd.sh "dump $1 $2" 8 | python3 -c "
import sys,struct,re
b=b''; base=None
for l in sys.stdin:
  if ': ' in l and l.startswith('0x'):
    a=int(l.split(':')[0],16)
    if base is None: base=a
    for w in l.split(': ')[1].split('|')[0].split(): b+=struct.pack('<I',int(w,16))
for m in re.finditer(rb'[\x20-\x7e]{4,}',b): print(hex(base+m.start()), '+%#x'%m.start(), m.group().decode())
"

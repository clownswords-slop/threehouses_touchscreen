#!/bin/bash
$HOME/cmd.sh "dump 0x2133776d60 96" 3 | python3 -c "
import sys,struct
b=b''
for l in sys.stdin:
  if ': ' in l and l.startswith('0x'):
    for w in l.split(': ')[1].split('|')[0].split(): b+=struct.pack('<I',int(w,16))
print(b[6:].split(b'\0')[0].decode('utf8','replace'))"

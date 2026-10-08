#!/bin/bash
# print hand x,y in 720p and owner
$HOME/cmd.sh "dump 0x2133774058 8;dump 0x2133774080 16" 3 | python3 -c "
import sys,struct
L=sys.stdin.read().split('\n')
for l in L:
  if l.startswith('0x2133774058'):
    w=l.split(':')[1].split('|')[0].split(); x=struct.unpack('f',struct.pack('I',int(w[1],16)))[0]; y=struct.unpack('f',struct.pack('I',int(w[2],16)))[0]; print('hand720 %.1f %.1f'%(x/1.5,y/1.5), end=' ')
  if l.startswith('0x2133774080'):
    w=l.split(':')[1].split('|')[0].split(); print('on=%s owner=0x%s%08x'%(int(w[0],16)&255, w[3][2:], int(w[2],16)))
"

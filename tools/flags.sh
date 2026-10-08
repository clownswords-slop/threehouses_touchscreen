#!/bin/bash
# flags.sh y0 y1 : print cell flag byte +0x26 for rows y0..y1, x 0..23
c=$HOME/cmd.sh
base=$($c "dump 0x9fc6f18 8" | tail -1 | awk '{print $3 $2}' | sed 's/0x//g')
for y in $(seq $1 $2); do a=$(printf "0x%x" $((0x$base + y*0x600))); 
 $c "dump $a $((24*0x30))" 5 | grep "^0x" | python3 -c "
import sys
ws=[]
for l in sys.stdin:
  ws+= [int(w,16) for w in l.split(':')[1].split('|')[0].split()]
b=b''.join(w.to_bytes(4,'little') for w in ws)
print('%2d '%$y+' '.join('%02x'%b[i*0x30+0x26] if b[i*0x30:i*0x30+8]==bytes(8) else '%02X'%b[i*0x30+0x26] for i in range(24)))
"; done

#!/bin/bash
# walkto.sh GX GZ [steps] : steer Byleth (free roam) toward world (GX, GZ) with the left-stick override; camera eye/look-at from main+0x1ab8480
c=$HOME/cmd.sh; GX=$1; GZ=$2; N=${3:-40}
cam=$($c "dump $((0x8504000+0x1ab8480)) 8" | tail -1 | awk '{printf "0x%x%08x", strtonum($3), strtonum($2)}')
last=""; stuck=0
for i in $(seq 1 $N); do
  v=$($c "dump $((cam+0x80)) 0x20" | grep -v seq | awk '{print $7, $9}' | tr '\n' ' ')
  read ex ez tx tz <<< "$v"
  cmd=$(python -c "
import math
ex,ez,tx,tz,gx,gz=$ex,$ez,$tx,$tz,$GX,$GZ
fx,fz=tx-ex,tz-ez; n=math.hypot(fx,fz); fx/=n; fz/=n; rx,rz=-fz,fx
dx,dz=gx-tx,gz-tz; d=math.hypot(dx,dz)
if d<500: print('DONE %d'%d)
else:
  sy=(dx*fx+dz*fz)/d; sx=(dx*rx+dz*rz)/d
  print('ls %d %d 45 %d'%(sx*30000,sy*30000,d))")
  set -- $cmd
  if [ "$1" = DONE ]; then echo "arrived dist=$2 at $tx $tz"; exit 0; fi
  $c "ls $2 $3 $4" >/dev/null; sleep 0.8
  echo "step $i at $tx,$tz dist=$5"
done

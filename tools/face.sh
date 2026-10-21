#!/bin/bash
# face.sh DX DZ : turn the free-roam camera until it looks along world (DX, DZ)
c=$HOME/cmd.sh; cam=$($c "dump $((0x8504000+0x1ab8480)) 8" | tail -1 | awk '{printf "0x%x%08x", strtonum($3), strtonum($2)}')
for i in $(seq 1 40); do
  v=$($c "dump $((cam+0x80)) 0x20" | grep -v seq | awk '{print $7, $9}' | tr '\n' ' ')
  r=$(python -c "
import math
ex,ez,tx,tz=map(float,'$v'.split()); fx,fz=tx-ex,tz-ez; n=math.hypot(fx,fz); fx/=n; fz/=n
gx,gz=$1,$2; m=math.hypot(gx,gz); gx/=m; gz/=m
dot=fx*gx+fz*gz; cr=fx*gz-fz*gx
print('ok' if dot>0.97 else ('1' if cr>0 else '-1'))")
  [ "$r" = ok ] && { echo faced; exit 0; }
  $c "rs $((r*16000)) 0 5" >/dev/null; sleep 0.35
done; echo "not faced"

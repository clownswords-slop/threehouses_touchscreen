#!/bin/bash
# mvw.sh x y tx ty : move the unit at x,y to tx,ty and Wait
c=$HOME/cmd.sh
$c "tt $1 $2" >/dev/null; sleep 1.2; $c "tt $3 $4" >/dev/null
for i in $(seq 1 20); do sleep 0.5; p=$($c base | grep -o 'phase=[0-9]*'); [ "$p" = "phase=2" ] && break; done
H=$($c base | grep -o 'hud=0x[0-9a-f]*' | cut -d= -f2); r=$($c "wl $H 0x1068"); cnt=$(echo "$r" | grep -o 'cnt=[0-9]*' | cut -d= -f2)
y=$((325 + ( (cnt-1) - (cnt-1)/2 )*66)); $c "tap 1111 $y" >/dev/null; sleep 2.5; $c base | tail -1

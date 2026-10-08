#!/bin/bash
# mv.sh ux uy tx ty [tx2 ty2 ...] : select unit, try destinations in order until the action menu opens, then Wait
c=$HOME/cmd.sh; ux=$1; uy=$2; shift 2
$c "tt $ux $uy" 3 >/dev/null; sleep 1.5
while [ $# -ge 2 ]; do
  $c "tt $1 $2" 3 >/dev/null; sleep 2.5
  p=$($c base 3 | grep -o 'phase=[0-9]*' | cut -d= -f2)
  if [ "$p" = "2" ]; then echo "moved to $1,$2"; [ -z "$NOWAIT" ] && { n=$($c "wl 0x2148959b30 0x1068" 3 | grep -o 'cnt=[0-9]*' | cut -d= -f2); y=$(python3 -c "print(int(325+66*(($n-1)-($n-1)//2)))"); $c "tap 1095 $y" 3 >/dev/null; sleep 2.5; }; exit 0; fi
  shift 2
done
echo "no move, phase $p"; $c "btn 2 8" 3 >/dev/null; sleep 1

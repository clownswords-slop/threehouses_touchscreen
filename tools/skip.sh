#!/bin/bash
# skip.sh N : N rounds of (press +, pick Yes on a Skip? dialog, else tap centre). Stops at a save/load list.
c=$HOME/cmd.sh; N=${1:-20}
for i in $(seq 1 $N); do
  h=$($c "dump 0x2133774058 8;dump 0x2133774080 4" 2)
  if echo "$h" | grep -q "0x43670000" && echo "$h" | grep -q "^0x2133774080: 0x1 "; then echo "save/load list at round $i"; exit 2; fi
  $c "btn 0x400 8" 2 >/dev/null; sleep 1.2
  y=$($c "dump 0x2133774e20 4;dump 0x2133774e80 16" 2)
  vis=$(echo "$y" | awk '/^0x2133774e20:/{print $2}'); visb=$(( (vis>>8)&255 ))
  if echo "$y" | grep -q "^0x2133774e80: 0x[01] 0x[0-9a-f]* 0x2 "; then
     $c "tap 508 419" 2 >/dev/null; echo "yes $i"; sleep 3
  else $c "tap 640 420" 2 >/dev/null; sleep 1.2; fi
done

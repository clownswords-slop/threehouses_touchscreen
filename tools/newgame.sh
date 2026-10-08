#!/bin/bash
# boot -> New Game -> Normal/Casual -> offline -> skip intro -> stop at first non yes/no hand menu
c=$HOME/cmd.sh
for i in $(seq 1 40); do $c info 2 >/dev/null && break; sleep 1; done
for i in $(seq 1 25); do r=$($c "wl 0x2133774030 0xa0760" 3); echo "$r" | grep -q "vis=1" && break; $c "btn 1 10" 3 >/dev/null; sleep 2; done
$c "seti 0x2133814790 1 1" 3 >/dev/null; sleep 3
$c "tap 640 237" 3 >/dev/null; sleep 2.5
$c "tap 640 290" 3 >/dev/null; sleep 3
$c "tap 770 419" 3 >/dev/null; sleep 5
for i in $(seq 1 40); do
  h=$($HOME/hand.sh)
  if echo "$h" | grep -q "on=1 owner=0x2133774e30"; then $c "tap 508 419" 2 >/dev/null; sleep 3; continue; fi
  if echo "$h" | grep -q "on=1"; then echo "hand at round $i: $h"; exit 0; fi
  $c "btn 0x401 8" 2 >/dev/null; sleep 1.5
done

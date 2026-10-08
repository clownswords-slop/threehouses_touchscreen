#!/bin/bash
# skip cutscenes until a hand menu other than yes/no appears
c=$HOME/cmd.sh; N=${1:-20}
for i in $(seq 1 $N); do
  h=$($HOME/hand.sh)
  if echo "$h" | grep -q "on=1 owner=0x2133774e30"; then echo "yesno $i"; $c "tap 508 419" 2 >/dev/null; sleep 3; continue; fi
  if echo "$h" | grep -q "on=1"; then echo "hand at round $i: $h"; exit 0; fi
  $c "btn 0x400 8" 2 >/dev/null; sleep 1.5
done

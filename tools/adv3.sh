#!/bin/bash
# advance: Yes/No -> No, else tap the middle; stop when the hand lands on something other than Yes/No
for i in $(seq 1 ${1:-10}); do
  yn=$(./cmd.sh "wl 0x2133774030 0xe00" | tail -1)
  h=$(./hand.sh); o=$(echo "$h" | grep -o 'owner=0x[0-9a-f]*' | cut -d= -f2); w=$(./cmd.sh "wl $o 0" | tail -1)
  tut=$(./cmd.sh "dump 0x2133a64311 1" | tail -1)
  echo "$i | $h | $w | yn $yn | tut $tut"
  if echo "$yn" | grep -q "vis=1"; then ./cmd.sh "tap 870 470" >/dev/null
  elif echo "$h" | grep -q "on=1"; then echo STOP-hand; break
  else ./cmd.sh "tap 640 300" >/dev/null; fi
  sleep ${2:-2}
done

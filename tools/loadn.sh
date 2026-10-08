#!/bin/bash
# loadn.sh N : boot -> title -> Continue -> load File N (0 = autosave). Never saves.
c=$HOME/cmd.sh; N=$1
for i in $(seq 1 60); do $c info 2 >/dev/null && break; sleep 1; done
for i in $(seq 1 30); do r=$($c "wl 0x2133774030 0xa0760" 3); echo "$r" | grep -q "vis=1" && break; $c "btn 1 10" 3 >/dev/null; sleep 2; done
idx=$(echo "$r" | grep -o 'idx=[0-9]*' | cut -d= -f2); cnt=$(echo "$r" | grep -o 'cnt=[0-9]*' | cut -d= -f2); T=0; [ "$cnt" -ge 8 ] && T=1   # a Bookmark entry comes first when there is one
$c "tap 1190 $((279+80*(T-idx)))" 3 >/dev/null
for k in $(seq 1 15); do sleep 1.5; o=$($HOME/hand.sh | grep "on=1" | grep -o 'owner=0x[0-9a-f]*' | cut -d= -f2); [ -n "$o" ] && $c "wl $o 0" 3 | grep -q "vt=+0x1a3aee0" && break; o=""; done
[ -z "$o" ] && { echo "not on save list"; exit 1; }
$c "seti $o $N 1" 3 >/dev/null; sleep ${2:-25}; $c base 3 | tail -2

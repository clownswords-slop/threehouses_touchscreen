#!/bin/bash
# from game boot: title -> Continue -> load the highlighted (last used) file
c=$HOME/cmd.sh
for i in $(seq 1 40); do $c info 2 >/dev/null && break; sleep 1; done
for i in $(seq 1 25); do r=$($c "wl 0x2133774030 0xa0760" 3); echo "$r" | grep -q "vis=1" && break; $c "btn 1 10" 3 >/dev/null; sleep 2; done
idx=$(echo "$r" | grep -o 'idx=[0-9]*' | cut -d= -f2); $c "tap 1190 $((279-80*idx))" 3 >/dev/null; sleep 3
$c "btn 1 10" 3 >/dev/null; sleep 25; $c base 3 | tail -1

#!/bin/bash
# after the game is launched: wait for the debug channel, go title -> Continue -> File 3 -> load
c=$HOME/cmd.sh
for i in $(seq 1 40); do $c info 2 >/dev/null && break; sleep 1; done
G=""
for i in $(seq 1 40); do
  G=$($c base 3 | grep -o 'G=0x[0-9a-f]*' | cut -d= -f2)
  if [ -n "$G" ] && [ "$G" != "0x0" ]; then
    r=$($c "wl $G 0xa0760" 3); echo "$r" | grep -q "vis=1" && break
  fi
  $c "btn 1 10" 3 >/dev/null; sleep 2
done
echo "title menu: $r"
# go to Continue (idx 0)
idx=$(echo "$r" | grep -o 'idx=[0-9]*' | cut -d= -f2); for k in $(seq 1 ${idx:-0}); do $c "btn 0x2000 10" 3 >/dev/null; sleep 0.6; done
$c "btn 1 10" 3 >/dev/null; sleep 3
echo "on save list (assumes cursor starts on File 3, the last used)"; $c "btn 1 10" 3 >/dev/null; sleep 3; $c "btn 1 10" 3 >/dev/null

#!/bin/bash
# ys3.sh lo hi : one chunk, quick
c=$HOME/cmd.sh
$c "range $1 $2;snapf -400 400" 60 >/dev/null
$c "rs 16000 0 20" 3 >/dev/null; sleep 1.0; n=$($c "fdel2 0.11 0.15 6.6 8.4" 30 | sed -n 2p | grep -o '[0-9]*$')
[ "$n" = "0" ] && { echo "$1 0"; exit; }
sleep 0.3; $c same 30 >/dev/null
$c "rs -16000 0 20" 3 >/dev/null; sleep 1.0; $c "fdel2 0.11 0.15 6.6 8.4" 30 >/dev/null; sleep 0.3; $c same 30 >/dev/null
$c "rs 16000 0 20" 3 >/dev/null; sleep 1.0; $c "fdel2 0.11 0.15 6.6 8.4" 30 >/dev/null; sleep 0.3; $c same 30 >/dev/null
$c "rs -16000 0 20" 3 >/dev/null; sleep 1.0; n=$($c "fdel2 0.11 0.15 6.6 8.4" 30 | sed -n 2p)
echo "$1 $n"; $c "list 10" 5 | grep ^0x

#!/bin/bash
# ysearch2.sh lo hi : floats (|v|<400) whose change follows the camera yaw (0.13 rad = 7.46 deg steps)
c=$HOME/cmd.sh
$c "range $1 $2;snapf -400 400" 60 | tail -1 | tr '\n' ' '
for i in 1 2; do
 $c "rs 16000 0 20" 3 >/dev/null; sleep 1.2; $c "fdel2 0.11 0.15 6.6 8.4" 30 | sed -n 2p | tr '\n' ' '; sleep 0.4; $c same 30 | tail -1 | tr '\n' ' '
 $c "rs -16000 0 20" 3 >/dev/null; sleep 1.2; $c "fdel2 0.11 0.15 6.6 8.4" 30 | sed -n 2p | tr '\n' ' '; sleep 0.4; $c same 30 | tail -1 | tr '\n' ' '
done
echo; $c "list 12" 5 | grep ^0x

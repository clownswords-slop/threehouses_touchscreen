#!/bin/bash
c=$HOME/cmd.sh
$c "tap 640 320" >/dev/null; sleep 3; $c "tap 500 425" >/dev/null; sleep 15; $c "tap 640 600" >/dev/null; sleep 5
for i in $(seq 1 30); do p=$($c base | grep -o 'phase=[0-9]*'); [ "$p" = "phase=0" ] && break; $c "tap 640 200" >/dev/null; sleep 1.5; done; $c base | tail -1

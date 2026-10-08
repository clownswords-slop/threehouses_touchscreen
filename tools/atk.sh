#!/bin/bash
# atk.sh <menu_y> <tx> <ty> : pick Attack at menu_y, confirm weapon, then tap target tile twice
c=$HOME/cmd.sh
$c "tap 1095 $1" 3 >/dev/null; sleep 2.5
$c "tap 300 500" 3 >/dev/null; sleep 2.5
$c "tt $2 $3" 3 >/dev/null; sleep 1.5
$c base 3 | tail -1
$c "tt $2 $3" 3 >/dev/null; sleep 8
$HOME/adv.sh 12

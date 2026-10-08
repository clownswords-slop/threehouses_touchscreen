#!/bin/bash
# vsearch.sh FN : FN in sin,nsin,cos,ncos. Track a float equal to FN(camera yaw) through rotations.
c=$HOME/cmd.sh
val(){ y=$($c ang 3 | grep -o 'yaw [-0-9.]*' | cut -d' ' -f2); python3 -c "import math;y=$y;print('%.4f'%{'sin':math.sin(y),'nsin':-math.sin(y),'cos':math.cos(y),'ncos':-math.cos(y)}['$1'])"; }
v=$(val $1); echo "start $v"; $c "range 0 0;scanf $v" 90 | tail -1
for s in 16000 -16000 16000 16000 -16000; do $c "rs $s 0 20" 3 >/dev/null; sleep 1.5; v=$(val $1); $c "filtf $v" 30 | tail -1; done
$c "list 20" 5

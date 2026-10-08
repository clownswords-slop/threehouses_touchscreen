#!/bin/bash
# atkat.sh ux uy mx my tx ty : move unit to (mx,my), choose first menu item (Attack), attack (tx,ty)
c=$HOME/cmd.sh
$c "tt $1 $2" 3 >/dev/null; sleep 1.5; $c "tt $3 $4" 3 >/dev/null; sleep 2.5
n=$($c "wl 0x2148959b30 0x1068" 3 | grep -o 'cnt=[0-9]*' | cut -d= -f2); y=$(python3 -c "print(int(325-66*(($n-1)//2)))")
echo "menu cnt $n"; $HOME/atk.sh $y $5 $6

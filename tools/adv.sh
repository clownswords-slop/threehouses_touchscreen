#!/bin/bash
# tap the screen centre until the player phase (0) comes back; N = max taps
c=$HOME/cmd.sh; N=${1:-20}
for i in $(seq 1 $N); do
  p=$($c base 3 | grep -o 'phase=[0-9-]*' | cut -d= -f2)
  [ "$p" = "0" ] && { echo "phase 0 after $i"; exit 0; }
  $c "tap 640 420" 3 >/dev/null; sleep 2
done
echo "still phase $p"

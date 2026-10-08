#!/bin/bash
# focus.sh k r c : put the status-sheet hand on node k, row r, column c
c=$HOME/cmd.sh; G=0x2133774030; h(){ printf '0x%x' $((G+$1)); }; CURP=$(h 0x602c8)
node=$((G+0x60178+$1*0x30))
L=$($c "dump $(printf '0x%x' $node) 16" 3 | grep '^0x' | awk '{printf "0x%s%08x", substr($3,3), strtonum($2)}')
pl=$($c "dump $CURP 8" 3 | grep '^0x' | awk '{printf "0x%s%08x", substr($3,3), strtonum($2)}')
po=$($c "dump $pl 8" 3 | grep '^0x' | awk '{printf "0x%s%08x", substr($3,3), strtonum($2)}')
$c "w8 $(printf '0x%x' $((po+8))) 0x30;w8 $(printf '0x%x' $((L+8))) 0x3c;w32 $(h 0x58) $(printf '0x%x' $((L&0xffffffff)));w32 $CURP $(printf '0x%x' $((node&0xffffffff)));call 0x5de6f0 $L $3 0 0;call 0x5de5b0 $L $2 0 0" 3 >/dev/null
sleep 0.8; $HOME/hand.sh

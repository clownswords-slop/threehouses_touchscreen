#!/bin/bash
# hand + owner widget info
h=$($HOME/hand.sh); echo "$h"; o=$(echo "$h" | grep -o 'owner=0x[0-9a-f]*' | cut -d= -f2)
[ -n "$o" ] && $HOME/cmd.sh "wl $o 0" 3 | tail -1 | sed "s/^/owner G+$(python3 -c "print(hex($o-0x2133774030))") /"

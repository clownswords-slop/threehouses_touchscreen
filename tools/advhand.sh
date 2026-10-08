#!/bin/bash
# adv: tap middle until a hand is on (max n taps)
n=${1:-30}
for i in $(seq 1 $n); do h=$($HOME/hand.sh); echo "$h" | grep -q "on=1" && { echo "hand after $i: $h"; exit 0; }; $HOME/cmd.sh "tap 640 300" 2 >/dev/null; sleep 1.2; done; echo "no hand: $h"

#!/bin/bash
# advance; auto-answer student questions (first option) and goal updates (No); stop on anything else with a hand
for k in $(seq 1 8); do
  $HOME/advhand.sh ${1:-40} >/dev/null
  o=$($HOME/hand.sh | grep "on=1" | grep -o 'owner=0x[0-9a-f]*' | cut -d= -f2); [ -z "$o" ] && { echo "no hand"; exit 1; }
  vt=$($HOME/cmd.sh "wl $o 0" 2 | tail -1)
  case "$vt" in *1a1db48*) echo "question"; $HOME/cmd.sh "tap 642 387" 2 >/dev/null; sleep 2;; *1a1e2d8*) echo "goals->No"; $HOME/cmd.sh "tap 769 580" 2 >/dev/null; sleep 2;; *) echo "stop: $o $vt"; exit 0;; esac
done

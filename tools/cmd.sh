#!/bin/bash
# usage: cmd.sh "<cmds separated by ;>" [timeout_s]
D=$HOME/mnt/Roaming--Ryujinx/sdcard/touchmod
SEQ=1$(date +%s%N | cut -c9-15)
printf '%s\n%s\n' "$SEQ" "$(echo "$1" | tr ';' '\n')" > $D/cmd.txt
T=${2:-5}; for i in $(seq 1 $((T*10))); do sleep 0.1; if head -1 $D/out.txt 2>/dev/null | grep -q "seq $SEQ"; then sleep 0.2; cat $D/out.txt; exit 0; fi; done
echo "TIMEOUT"; exit 1

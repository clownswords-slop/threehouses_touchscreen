#!/bin/bash
# tail touchmod log lines
f=$(ls -t $HOME/mnt/emulators--Ryujinx/Logs/*.log | head -1); grep -a "touchmod" "$f" | tail -${1:-15} | cut -c1-220

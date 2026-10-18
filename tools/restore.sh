#!/bin/bash
# restore every save file (both copies) from the session-20 backup; run only with emulation stopped
S=$HOME/mnt/Roaming--Ryujinx/bis/user/save/0000000000000004; B=$HOME/mnt/Roaming--Ryujinx/claude_save_backup/0000000000000004_20261018_session20
for d in 0 1; do for f in $(ls $B/$d); do cp $B/$d/$f $S/$d/$f; done; done
for d in 0 1; do for f in $(ls $B/$d); do cmp -s $B/$d/$f $S/$d/$f || echo "DIFF $d/$f"; done; done; echo restored

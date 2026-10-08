#!/bin/bash
# after Stop Emulation: copy debug build if given
M=$HOME/mnt/Roaming--Ryujinx/mods/contents/010055d009f78000/TouchControls/exefs
sleep 10; [ -n "$1" ] && cp $HOME/mnt/emulators--Ryujinx/claude_touchmod/debug/subsdk9 $M/subsdk9; md5sum $M/subsdk9

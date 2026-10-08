#!/bin/bash
# wait for the game to stop (cmd channel silent), then copy the new debug build into the mods folder
M=$HOME/mnt/Roaming--Ryujinx/mods/contents/010055d009f78000/TouchControls/exefs
sleep ${1:-8}; cp $HOME/mnt/emulators--Ryujinx/claude_touchmod/debug/subsdk9_new $M/subsdk9 && md5sum $M/subsdk9

#!/bin/bash
# loadf.sh N : (game booting) wait for the title menu, Continue, select File N (index N on the save list), confirm. Never saves.
c=$HOME/cmd.sh; N=$1
for i in $(seq 1 60); do $c info 2 >/dev/null && break; sleep 1; done
# the common UI object G moves between boots: read it from the mod (base prints G=0x...)
for i in $(seq 1 30); do G=$($c base 3 | grep -o 'G=0x[0-9a-f]*' | cut -c3-); [ -n "$G" ] && [ "$G" != "0x0" ] && break; sleep 1; done
[ -z "$G" ] && { echo "no G"; exit 1; }
GT=$(printf '0x%x' $((G+0x58)))
for i in $(seq 1 40); do r=$($c "wl $G 0xa0760" 3); echo "$r" | grep -q "vis=1" && break; [ $i -gt 8 ] && $c base 3 | grep -q "hud=0x0 " && $c "btn 1 8" 3 >/dev/null; sleep 2; done   # A only before any save is loaded (no battle HUD)
echo "$r" | grep -q "vis=1" || { echo "no title menu (a save may already be loaded: return to the title first)"; exit 1; }

$c "tap 1190 279" 3 >/dev/null   # selected entry = Continue (title opens on it)
for k in $(seq 1 15); do sleep 1; o=$($c "dump $GT 8" 3 | grep -v seq | awk '{printf "0x%x%08x", strtonum($3), strtonum($2)}'); $c "wl $o 0" 3 | grep -q "vt=+0x1a3aee0" && break; o=""; done
[ -z "$o" ] && { echo "not on save list"; exit 1; }
$c "seti $o $N 1" 3 >/dev/null; sleep ${2:-28}; $c base 3 | tail -2

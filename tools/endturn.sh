#!/bin/bash
# open map menu on an empty tile and choose End
c=$HOME/cmd.sh; T=${1:-"12 18"}
$c "tt $T" 3 >/dev/null; sleep 1.5; $c "tt $T" 3 >/dev/null; sleep 2
$c "tap 1095 523" 3 >/dev/null; sleep 2
$c base 3 | tail -1

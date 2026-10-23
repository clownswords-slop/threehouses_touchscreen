#!/bin/bash
# opts.sh : from the battle prep wheel (phase 28), open System -> Options
c=$HOME/cmd.sh
$c "tap 800 470" >/dev/null; sleep 1.5; $c "tap 800 470" >/dev/null; sleep 2; $c "tap 940 467" >/dev/null; sleep 3
$c "wl 0x2133774030 0x2250" | tail -1

#!/bin/bash
# sample the battle cursor n times (~0.15 s apart)
for i in $(seq 1 ${1:-30}); do $HOME/cmd.sh base 1 | tail -1 | tr '\n' ' '; sleep 0.1; done; echo

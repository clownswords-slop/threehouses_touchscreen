#!/bin/bash
# measure hand positions (720p) for every reachable node of the current status tab; prints G-offset of each list
c=$HOME/cmd.sh; G=0x2133774030
h(){ printf '0x%x' $((G+$1)); }
CURP=$(h 0x602c8)
nodes=$($c "dump $(h 0x60178) 0x150" 3 | python3 -c "
import sys
ws=[]
for l in sys.stdin:
  if l.startswith('0x'): ws+=[int(x,16) for x in l.split(':')[1].split('|')[0].split()]
q=[(ws[i+1]<<32)|ws[i] for i in range(0,len(ws),2)]
N=[q[k*6:(k+1)*6] for k in range(7)]
selfs={n[0]:k for k,n in enumerate(N) if n[0]}
seen={0}; st=[0]
while st:
  k=st.pop()
  for p in N[k][1:5]:
    if p in selfs and selfs[p] not in seen: seen.add(selfs[p]); st.append(selfs[p])
print(' '.join(str(k) for k in sorted(seen)))
")
echo "reachable: $nodes"
for k in $nodes; do
  node=$((G+0x60178+k*0x30))
  L=$($c "dump $(printf '0x%x' $node) 16" 3 | grep '^0x' | awk '{printf "0x%s%08x", substr($3,3), strtonum($2)}')
  w=$($c "dump $(printf '0x%x' $((L+0x4c))) 16" 3)
  cnt=$(echo "$w" | grep '^0x' | awk '{print strtonum($4)}'); ccnt=$(echo "$w" | grep '^0x' | awk '{print strtonum($5)}'); [ "$ccnt" -lt 1 ] && ccnt=1
  pl=$($c "dump $CURP 8" 3 | grep '^0x' | awk '{printf "0x%s%08x", substr($3,3), strtonum($2)}')
  po=$($c "dump $pl 8" 3 | grep '^0x' | awk '{printf "0x%s%08x", substr($3,3), strtonum($2)}')
  $c "w8 $(printf '0x%x' $((po+8))) 0x30;w8 $(printf '0x%x' $((L+8))) 0x3c;w32 $(h 0x58) $(printf '0x%x' $((L&0xffffffff)));w32 $CURP $(printf '0x%x' $((node&0xffffffff)))" 3 >/dev/null
  line="off $(printf '0x%x' $((L-G))) node $k cnt=$cnt ccnt=$ccnt :"
  for r in $(seq 0 $((cnt-1))); do for cc in $(seq 0 $((ccnt-1))); do
    $c "call 0x5de6f0 $L $cc 0 0;call 0x5de5b0 $L $r 0 0" 3 >/dev/null; sleep 0.3
    p=$($c "dump $(h 0x2c) 8" 3 | grep '^0x' | awk "{printf \"%.1f,%.1f\", \$7/1.5,\$8/1.5}")
    line="$line $r/$cc=$p"
  done; done
  echo "$line"
done

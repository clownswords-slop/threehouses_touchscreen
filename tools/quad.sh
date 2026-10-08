#!/bin/bash
# usage: quad.sh "i j h" ... -> projected corners (screen coords in the 1456x819 frame)
c=$HOME/cmd.sh
rd64(){ $c "dump $1 0x10" 3 | tail -1 | awk '{print $2, $3}' | python3 -c "import sys;a=sys.stdin.read().split();print(hex(int(a[0],16)|int(a[1],16)<<32))"; }
CAM=$(rd64 0x9fbc480); PO=$(rd64 0x9fac198); PZ=$(rd64 0x9fac1a0)
M=$($c "dump $(python3 -c "print(hex($CAM+0x170))") 0x40" 3); O=$($c "dump $PO 0x10" 3|tail -1); Z=$($c "dump $PZ 0x10" 3|tail -1)
python3 - "$@" <<PY
import sys,struct
M="""$M"""; O="""$O"""; Z="""$Z"""
m=[]
for l in M.split('\n'):
  if l.startswith('0x'): m+= [struct.unpack('<f',struct.pack('<I',int(w,16)))[0] for w in l.split(': ')[1].split('|')[0].split()]
f=lambda s: struct.unpack('<f',struct.pack('<I',int(s.split(': ')[1].split()[0],16)))[0]
ox,oz=f(O),f(Z)
def pr(x,y,z):
  cx=x*m[0]+y*m[4]+z*m[8]+m[12]; cy=x*m[1]+y*m[5]+z*m[9]+m[13]; cw=x*m[3]+y*m[7]+z*m[11]+m[15]
  sx=(cx/cw*0.5+0.5)*1280; sy=(0.5-cy/cw*0.5)*720
  return (round(72+1.027*sx), round(24+1.027*sy))
for a in sys.argv[1:]:
  i,j,h=[float(v) for v in a.split()]
  x=ox+(i+0.5)*1000; z=oz+(j+0.5)*1000
  print(a, [pr(x+dx,h,z+dz) for dx,dz in ((-500,-500),(500,-500),(500,500),(-500,500))])
PY

#!/bin/bash
# ss.sh NAME [x0 y0 x1 y1]: wait for a fresh Ryujinx screenshot (F8) and convert to 1280x720 jpg screenshots/NAME.jpg
D="$HOME/mnt/Roaming--Ryujinx/screenshots"
for i in $(seq 1 40); do f=$(ls -t "$D"/*.png | head -1); a=$(( $(date +%s) - $(stat -c %Y "$f") )); s1=$(stat -c %s "$f"); sleep 0.5; s2=$(stat -c %s "$f"); if [ $a -lt 15 ] && [ "$s1" = "$s2" ] && [ $s1 -gt 100000 ]; then break; fi; done
python3 - "$f" "$D/$1.jpg" "${@:2}" <<'P'
import sys
from PIL import Image
im=Image.open(sys.argv[1]).convert('RGB').resize((1280,720))
a=sys.argv[3:]
if a:
  x0,y0,x1,y1=map(int,a); im=im.crop((x0,y0,x1,y1))
im.save(sys.argv[2],quality=85)
print(sys.argv[2], im.size)
P

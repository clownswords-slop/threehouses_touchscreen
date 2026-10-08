#!/bin/bash
# minimap crop around the cathedral, 3x
$HOME/ss.sh $1 ${2:-940} ${3:-100} ${4:-1160} ${5:-260} >/dev/null; python3 -c "
from PIL import Image; im=Image.open('$HOME/mnt/Roaming--Ryujinx/screenshots/$1.jpg'); im.resize((im.size[0]*2,im.size[1]*2)).save('$HOME/mnt/Roaming--Ryujinx/screenshots/$1.jpg')"

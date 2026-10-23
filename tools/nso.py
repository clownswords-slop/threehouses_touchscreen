import struct, lz4.block, sys
d=open(sys.argv[1],'rb').read()
flags=struct.unpack_from('<I',d,0xC)[0]
segs=[struct.unpack_from('<IIII',d,0x10+i*0x10) for i in range(3)]
csz=struct.unpack_from('<III',d,0x60)
bss=segs[2][3]
end=max(s[1]+s[2] for s in segs)
img=bytearray(end)
for i,(fo,mo,sz,_) in enumerate(segs):
    raw=d[fo:fo+csz[i]]
    data=lz4.block.decompress(raw,uncompressed_size=sz) if flags&(1<<i) else raw[:sz]
    img[mo:mo+sz]=data
    print(i,hex(mo),hex(sz))
open('main.bin','wb').write(img)

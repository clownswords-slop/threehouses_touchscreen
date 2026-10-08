import struct,sys,hashlib
e=open(sys.argv[1],'rb').read()
phoff=struct.unpack_from('<Q',e,0x20)[0]; phnum=struct.unpack_from('<H',e,0x38)[0]
loads=[]
for i in range(phnum):
    t,fl,off,va,pa,fsz,msz,al=struct.unpack_from('<IIQQQQQQ',e,phoff+i*0x38)
    if t==1: loads.append((va,e[off:off+fsz],msz))
assert len(loads)==3,loads
segs=[]
for i,(va,data,msz) in enumerate(loads):
    if i<2: data=data+b'\0'*(msz-len(data))
    segs.append((va,data,msz))
dva,ddata,dmsz=segs[2]
bss=(dva+dmsz)-(dva+len(ddata))
bss=(bss+0xfff)&~0xfff
hdr=bytearray(0x100); struct.pack_into('<4sIII',hdr,0,b'NSO0',0,0,0x38)  # flags: hash check bits 3,4,5
off=0x100; body=b''
for i,(va,data,msz) in enumerate(segs):
    struct.pack_into('<III',hdr,0x10+i*0x10,off+len(body),va,len(data))
    struct.pack_into('<I',hdr,0x60+i*4,len(data))
    struct.pack_into('32s',hdr,0xA0+i*0x20,hashlib.sha256(data).digest())
    body+=data
struct.pack_into('<I',hdr,0x3C,bss)
struct.pack_into('32s',hdr,0x40,hashlib.sha256(body).digest()[:20]+b'\0'*12)
open(sys.argv[2],'wb').write(hdr+body)
print('ok',[(hex(v),hex(len(d))) for v,d,m in segs],'bss',hex(bss))

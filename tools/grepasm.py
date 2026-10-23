import capstone,sys,re
img=open('main.bin','rb').read()
md=capstone.Cs(capstone.CS_ARCH_ARM64,capstone.CS_MODE_ARM); md.skipdata=True
pat=re.compile(sys.argv[1])
out=open('main.asm','w') if len(sys.argv)>2 else None
for i in md.disasm(img[0:0xafe320],0):
    s=f"{i.address:x} {i.mnemonic} {i.op_str}"
    if out: out.write(s+'\n')
    if pat.search(s): print(s)

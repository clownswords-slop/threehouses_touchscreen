# mknpdm.py IN OUT [--fs-all]
# The mod's main.npdm = the game's own (work/main.npdm) plus, in both the ACID (what may be granted) and the ACI0 (what is
# requested) blocks:
#   - filesystem permission bit 21 (SdCard; --fs-all sets every bit, as the old debug npdm did)
#   - the memory SVCs used to write hooks into the game's code: 0x73 SetProcessMemoryPermission, 0x74/0x75 Map/UnmapProcessMemory,
#     0x77/0x78 Map/UnmapProcessCodeMemory, and 0x40 CreateSession / 0x43 ReplyAndReceive to obtain a real handle to our own
#     process (the pseudo-handle 0xFFFF8001 is refused by these SVCs): the handle is sent to ourselves in an IPC message
# Kernel capability "EnableSystemCalls" descriptors: low 5 bits 0b01111, bits 5-28 = mask of 24 SVCs, bits 29-31 = index (svc / 24).
import struct, sys

SVCS = [0x40, 0x43, 0x73, 0x74, 0x75, 0x77, 0x78]
SDCARD = 1 << 21

def new_caps(words):
    want = {}
    for s in SVCS: want.setdefault(s // 24, 0); want[s // 24] |= 1 << (s % 24)
    last = -1
    for i, w in enumerate(words):
        if (w & 0x1f) == 0x0f:
            last = i; idx = (w >> 29) & 7
            if idx in want: words[i] = w | (want.pop(idx) << 5)
    add = [(idx << 29) | (mask << 5) | 0x0f for idx, mask in sorted(want.items())]
    return words[:last + 1] + add + words[last + 1:]   # new SVC descriptors right after the existing ones

def main():
    src, dst = sys.argv[1], sys.argv[2]; fs_all = '--fs-all' in sys.argv
    d = bytearray(open(src, 'rb').read())
    assert d[:4] == b'META'
    aci_off, aci_sz, acid_off, acid_sz = struct.unpack_from('<IIII', d, 0x70)
    blocks = {}
    for name, base, hdr in (('acid', acid_off, 0x220), ('aci', aci_off, 0x20)):
        fs_off, fs_sz, srv_off, srv_sz, kc_off, kc_sz = struct.unpack_from('<IIIIII', d, base + hdr)
        size = acid_sz if name == 'acid' else aci_sz
        assert kc_off + kc_sz == size, 'kernel caps must be the last part of the block'
        perms = struct.unpack_from('<Q', d, base + fs_off + 4)[0]
        struct.pack_into('<Q', d, base + fs_off + 4, 0xffffffffffffffff if fs_all else perms | SDCARD)
        words = [struct.unpack_from('<I', d, base + kc_off + i)[0] for i in range(0, kc_sz, 4)]
        words = new_caps(words)
        struct.pack_into('<I', d, base + hdr + 0x14, len(words) * 4)
        blocks[name] = bytes(d[base:base + kc_off]) + b''.join(struct.pack('<I', w) for w in words)
    # rebuild: META header, ACID (0x10-aligned), ACI0
    acid = bytearray(blocks['acid']); struct.pack_into('<I', acid, 0x204, len(acid) - 0x100)   # ACID size field excludes the signature
    out = bytearray(d[:acid_off]) + acid
    while len(out) % 0x10: out += bytes(1)
    new_aci_off = len(out); out += blocks['aci']
    struct.pack_into('<IIII', out, 0x70, new_aci_off, len(blocks['aci']), acid_off, len(acid))
    open(dst, 'wb').write(out)

main()

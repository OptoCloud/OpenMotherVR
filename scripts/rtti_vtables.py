# OpenMotherVR-Provenance: clean
import sys, pefile, struct
pe = pefile.PE(sys.argv[1]); B = pe.OPTIONAL_HEADER.ImageBase
img = pe.get_memory_mapped_image()
rd = next(s for s in pe.sections if s.Name.startswith(b'.rdata'))
lo, hi = rd.VirtualAddress, rd.VirtualAddress + rd.Misc_VirtualSize
txt = next(s for s in pe.sections if s.Name.startswith(b'.text'))
def u32(rva): return struct.unpack_from('<I', img, rva)[0]
def is_text(va): return txt.VirtualAddress <= va-B < txt.VirtualAddress+txt.Misc_VirtualSize
for rva in range(lo, hi-4, 4):
    col = u32(rva) - B
    if not (lo <= col < hi): continue
    if u32(col) != 0: continue  # signature 0 on x86
    td = u32(col+12) - B
    if not (0 < td < len(img)): continue
    name = img[td+8:td+8+128].split(b'\0')[0]
    if not name.startswith(b'.?A'): continue
    vt = rva + 4; n = 0
    while is_text(u32(vt + 4*n)): n += 1
    if n == 0: continue
    print(f'{B+vt:08x} {name.decode():60s} n={n} off={u32(col+4):#x}  ' + ' '.join(f'{u32(vt+4*i):08x}' for i in range(min(n,40))))

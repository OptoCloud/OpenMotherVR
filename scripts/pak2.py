"""Alien: Isolation PAK2 archive tool (list / extract / diff).

Layout (little-endian):
    char[4] magic = "PAK2"
    u32     names_size        bytes of NUL-terminated names following the header
    u32     count
    u32     alignment
    char    names[names_size]
    u32     end_offset[count] absolute file offset where entry i ends
    ...     data              entry 0 starts right after the offset table,
                              entry i>0 starts at end_offset[i-1]; either way
                              the start is rounded up to `alignment` (the gap
                              is zero padding, not part of the entry)

usage:
    pak2.py list    <pak>
    pak2.py extract <pak> <outdir>
    pak2.py diff    <stock.pak> <modded.pak>
"""
import hashlib
import os
import struct
import sys


def read_pak(path):
    with open(path, "rb") as f:
        b = f.read()
    magic, names_size, count, align = struct.unpack_from("<4sIII", b, 0)
    if magic != b"PAK2":
        raise ValueError(f"{path}: not a PAK2 archive")
    names = [n.decode() for n in b[16:16 + names_size].split(b"\0")[:count]]
    table = 16 + names_size
    ends = struct.unpack_from(f"<{count}I", b, table)
    start = table + 4 * count
    entries = {}
    for name, end in zip(names, ends):
        start = (start + align - 1) // align * align
        entries[name] = b[start:end]
        start = end
    return entries


def main(argv):
    cmd = argv[1]
    if cmd == "list":
        for name, data in read_pak(argv[2]).items():
            print(f"{len(data):10}  {name}")
    elif cmd == "extract":
        out = argv[3]
        for name, data in read_pak(argv[2]).items():
            p = os.path.join(out, *name.split("/"))
            os.makedirs(os.path.dirname(p), exist_ok=True)
            with open(p, "wb") as f:
                f.write(data)
        print(f"extracted to {out}")
    elif cmd == "diff":
        stock, mod = read_pak(argv[2]), read_pak(argv[3])
        sha = lambda d: hashlib.sha256(d).hexdigest()
        for name in sorted(set(stock) | set(mod)):
            if name not in stock:
                print(f"ADDED     {len(mod[name]):10}  {name}")
            elif name not in mod:
                print(f"REMOVED   {len(stock[name]):10}  {name}")
            elif sha(stock[name]) != sha(mod[name]):
                print(f"CHANGED   {len(stock[name]):10} -> {len(mod[name]):10}  {name}")
    else:
        print(__doc__)


if __name__ == "__main__":
    main(sys.argv)

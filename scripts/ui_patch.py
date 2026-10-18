"""Build or apply the UI archive patch: the mod's options menu as a delta
against the player's own DATA/UI.PAK, so no game files are distributed.

    python -I scripts/ui_patch.py make  <stock UI.PAK> <modded UI.PAK> <out.bin>
    python -I scripts/ui_patch.py apply <stock UI.PAK> <patch.bin> <out.PAK> [<variant tag>]
    python -I scripts/ui_patch.py check <stock UI.PAK> <modded UI.PAK> <patch.bin>

Patch format (little-endian), read by src/core/ui/ui_patch.cpp:

    char[8] magic "MVRUIP01"
    u32     alignment        of the output PAK2 archive
    u32     count            entries in the output, in output order
    entry   * count:
        u16   name length, then the name bytes (no NUL)
        u8    kind: 0 = the stock entry unchanged
                    1 = delta from the stock entry
                    2 = new entry, bytes follow
        kind 0/1: u32 stock size, u32 stock CRC-32 (zlib)
        kind 1:   u32 output size, u32 op count, ops:
                      u8 0 (copy):   u32 stock offset, u32 length
                      u8 1 (insert): u32 length, then the bytes
        kind 2:   u32 size, then the bytes

A variant entry (spec 23 §6.2) is named "<entry>|<tag>" and follows the
entry it can replace; kinds 0 and 1 then refer to the stock entry <entry>.
The DLL puts the variant whose tag it asks for in place of <entry> and drops
the others; apply does the same with the optional tag.
"""
import struct
import sys
import zlib
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
import pak2  # noqa: E402
from pak2 import pak_alignment, write_pak  # noqa: E402  (re-exported: they were defined here)

MAGIC = b"MVRUIP01"
BLOCK = 16
VARIANT = "|"

# Edits to the modded UI before diffing, as length-prefixed strings in the
# movies' ActionScript string tables (same length, so nothing moves): the
# original's "Oculus" runtime entry selects OpenXR in this mod.
LABEL_EDITS = [
    (b"\x0aUse Oculus", b"\x0aUse OpenXR"),
    (b"\x06Oculus", b"\x06OpenXR"),
]


def edited(entries: dict[str, bytes]) -> dict[str, bytes]:
    out = {}
    for name, data in entries.items():
        for old, new in LABEL_EDITS:
            if data.count(old) == 1:
                data = data.replace(old, new)
        out[name] = data
    return out


def base_name(name: str) -> str:
    """The entry a variant entry replaces; the name itself otherwise."""
    return name.split(VARIANT, 1)[0]


def delta(base: bytes, target: bytes) -> list[tuple]:
    """Greedy block matching: copy runs of base, insert the rest."""
    index: dict[bytes, int] = {}
    for i in range(len(base) - BLOCK + 1):
        index.setdefault(base[i:i + BLOCK], i)
    ops: list[tuple] = []
    pending = bytearray()
    j = 0
    while j < len(target):
        at = index.get(target[j:j + BLOCK]) if j + BLOCK <= len(target) else None
        if at is None:
            pending.append(target[j])
            j += 1
            continue
        n = BLOCK
        while at + n < len(base) and j + n < len(target) and base[at + n] == target[j + n]:
            n += 1
        if pending:
            ops.append(("insert", bytes(pending)))
            pending.clear()
        if ops and ops[-1][0] == "copy" and ops[-1][1] + ops[-1][2] == at:
            ops[-1] = ("copy", ops[-1][1], ops[-1][2] + n)
        else:
            ops.append(("copy", at, n))
        j += n
    if pending:
        ops.append(("insert", bytes(pending)))
    return ops


def make(stock_path: Path, mod_path: Path) -> bytes:
    stock, mod = pak2.read_pak(str(stock_path)), edited(pak2.read_pak(str(mod_path)))
    out = bytearray(MAGIC)
    out += struct.pack("<II", pak_alignment(mod_path), len(mod))
    for name, data in mod.items():
        enc = name.encode("ascii")
        out += struct.pack("<H", len(enc)) + enc
        base = stock.get(base_name(name))
        if base is None:
            out += struct.pack("<BI", 2, len(data)) + data
        elif base == data:
            out += struct.pack("<BII", 0, len(base), zlib.crc32(base))
        else:
            ops = delta(base, data)
            out += struct.pack("<BIIII", 1, len(base), zlib.crc32(base), len(data), len(ops))
            for op in ops:
                if op[0] == "copy":
                    out += struct.pack("<BII", 0, op[1], op[2])
                else:
                    out += struct.pack("<BI", 1, len(op[1])) + op[1]
    return bytes(out)


def apply(stock_path: Path, patch: bytes) -> tuple[int, dict[str, bytes]]:
    stock = pak2.read_pak(str(stock_path))
    assert patch[:8] == MAGIC, "not a UI patch"
    align, count = struct.unpack_from("<II", patch, 8)
    p = 16
    entries: dict[str, bytes] = {}
    for _ in range(count):
        (n,) = struct.unpack_from("<H", patch, p); p += 2
        name = patch[p:p + n].decode("ascii"); p += n
        kind = patch[p]; p += 1
        if kind == 2:
            (size,) = struct.unpack_from("<I", patch, p); p += 4
            entries[name] = patch[p:p + size]; p += size
            continue
        size, crc = struct.unpack_from("<II", patch, p); p += 8
        base = stock[base_name(name)]
        if len(base) != size or zlib.crc32(base) != crc:
            raise SystemExit(f"{name}: the stock archive differs from the one the patch was made for")
        if kind == 0:
            entries[name] = base
            continue
        out_size, op_count = struct.unpack_from("<II", patch, p); p += 8
        data = bytearray()
        for _ in range(op_count):
            op = patch[p]; p += 1
            if op == 0:
                off, ln = struct.unpack_from("<II", patch, p); p += 8
                data += base[off:off + ln]
            else:
                (ln,) = struct.unpack_from("<I", patch, p); p += 4
                data += patch[p:p + ln]; p += ln
        assert len(data) == out_size
        entries[name] = bytes(data)
    return align, entries


def select_variant(entries: dict[str, bytes], tag: str | None) -> dict[str, bytes]:
    """The archive the DLL builds for `tag` (None: no variant)."""
    out: dict[str, bytes] = {}
    for name, data in entries.items():
        if VARIANT not in name:
            out[name] = data
        elif tag is not None and name.split(VARIANT, 1)[1] == tag:
            if base_name(name) not in out:
                raise SystemExit(f"{name}: no entry before it to replace")
            out[base_name(name)] = data
    return out


def main() -> None:
    cmd, a, b, c = sys.argv[1], Path(sys.argv[2]), Path(sys.argv[3]), Path(sys.argv[4])
    tag = sys.argv[5] if len(sys.argv) > 5 else None
    if cmd == "make":
        patch = make(a, b)
        c.write_bytes(patch)
        print(f"{c}: {len(patch)} bytes")
    elif cmd == "apply":
        align, entries = apply(a, b.read_bytes())
        entries = select_variant(entries, tag)
        write_pak(c, align, entries)
        print(f"{c}: {len(entries)} entries")
    elif cmd == "check":
        _, entries = apply(a, c.read_bytes())
        mod = edited(pak2.read_pak(str(b)))
        same = list(entries) == list(mod) and all(entries[k] == mod[k] for k in mod)
        print("patch reproduces every entry of the modded archive" if same else "MISMATCH")
        sys.exit(0 if same else 1)


if __name__ == "__main__":
    main()

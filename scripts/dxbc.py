# OpenMotherVR-Provenance: clean
"""DXBC container helpers: checksum and chunk listing.

    python -I scripts/dxbc.py <shader.bin> [...]

Prints each blob's chunks and whether its stored checksum matches the one
computed here (the D3D runtime rejects blobs whose checksum is wrong).
The checksum is MD5's compression function over the blob after the 20-byte
header, with DXBC's own final-block layout.
"""
import math
import pathlib
import struct
import sys

_S = [7, 12, 17, 22] * 4 + [5, 9, 14, 20] * 4 + [4, 11, 16, 23] * 4 + [6, 10, 15, 21] * 4
_K = [int(abs(math.sin(i + 1)) * 2**32) & 0xFFFFFFFF for i in range(64)]


def _rotl(x: int, c: int) -> int:
    return ((x << c) | (x >> (32 - c))) & 0xFFFFFFFF


def _md5_block(state: list[int], block: bytes) -> None:
    m = struct.unpack("<16I", block)
    a, b, c, d = state
    for i in range(64):
        if i < 16:
            f, g = (b & c) | (~b & d), i
        elif i < 32:
            f, g = (d & b) | (~d & c), (5 * i + 1) % 16
        elif i < 48:
            f, g = b ^ c ^ d, (3 * i + 5) % 16
        else:
            f, g = c ^ (b | ~d), (7 * i) % 16
        f = (f + a + _K[i] + m[g]) & 0xFFFFFFFF
        a, d, c = d, c, b
        b = (b + _rotl(f, _S[i])) & 0xFFFFFFFF
    for i, v in enumerate((a, b, c, d)):
        state[i] = (state[i] + v) & 0xFFFFFFFF


def checksum(blob: bytes) -> bytes:
    data = blob[20:]
    size = len(data)
    state = [0x67452301, 0xEFCDAB89, 0x98BADCFE, 0x10325476]
    full = size - size % 64
    for off in range(0, full, 64):
        _md5_block(state, data[off:off + 64])
    left = data[full:]
    num_bits = (size * 8) & 0xFFFFFFFF
    num_bits_part = ((num_bits >> 2) | 1) & 0xFFFFFFFF
    if len(left) >= 56:
        block = bytearray(64)
        block[:len(left)] = left
        block[len(left)] = 0x80
        _md5_block(state, bytes(block))
        block = bytearray(64)
        struct.pack_into("<I", block, 0, num_bits)
        struct.pack_into("<I", block, 60, num_bits_part)
        _md5_block(state, bytes(block))
    else:
        block = bytearray(64)
        struct.pack_into("<I", block, 0, num_bits)
        block[4:4 + len(left)] = left
        block[4 + len(left)] = 0x80
        struct.pack_into("<I", block, 60, num_bits_part)
        _md5_block(state, bytes(block))
    return struct.pack("<4I", *state)


def chunks(blob: bytes) -> list[tuple[str, int, int]]:
    count = struct.unpack_from("<I", blob, 28)[0]
    out = []
    for i in range(count):
        off = struct.unpack_from("<I", blob, 32 + 4 * i)[0]
        tag = blob[off:off + 4].decode("ascii", "replace")
        size = struct.unpack_from("<I", blob, off + 4)[0]
        out.append((tag, off + 8, size))
    return out


def main() -> None:
    for name in sys.argv[1:]:
        blob = pathlib.Path(name).read_bytes()
        ok = checksum(blob) == blob[4:20]
        print(f"{name}: {len(blob)} bytes, checksum {'ok' if ok else 'MISMATCH'}, "
              f"chunks {[c[0] for c in chunks(blob)]}")


if __name__ == "__main__":
    main()

"""Disassemble a range of a 32-bit PE.

    python -I scripts/disasm.py <pe> <address> [length] [--rva]

<address> is a VA (hex) unless --rva is given. Length defaults to 0x80 bytes.
Addresses in the output are VAs at the file's preferred image base
(dxgi.dll: 0x10000000, AI.exe: 0x00400000).

Needs: pip install pefile capstone
"""
import sys

import capstone
import pefile


def main() -> None:
    args = [a for a in sys.argv[1:] if not a.startswith("--")]
    rva_mode = "--rva" in sys.argv
    pe = pefile.PE(args[0], fast_load=True)
    base = pe.OPTIONAL_HEADER.ImageBase
    addr = int(args[1], 16)
    length = int(args[2], 16) if len(args) > 2 else 0x80
    va = base + addr if rva_mode else addr
    data = pe.get_data(va - base, length)
    md = capstone.Cs(capstone.CS_ARCH_X86, capstone.CS_MODE_32)
    for i in md.disasm(data, va):
        print(f"{i.address:08x}  {i.bytes.hex():<20} {i.mnemonic} {i.op_str}")


if __name__ == "__main__":
    main()

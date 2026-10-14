"""Resolve the original's AI.exe address globals and show what is there.

    python -I scripts/ai_slot.py <AI.exe> DAT_10066d64 [DAT_... ...] [--2020]

Each DAT_10066bc8 + 4*i global in the original dxgi.dll holds the AI.exe
address for slot i, rebased at startup. The per-build RVAs are in
src/hook/ai_address_tables.inc; this prints slot, RVA and the code or
data found there in the given AI.exe (2015 table unless --2020).
"""
import pathlib
import re
import struct
import sys

import capstone
import pefile

INC = pathlib.Path(__file__).resolve().parent.parent / "src/hook/ai_address_tables.inc"


def table(use_2020: bool) -> list[int]:
    text = INC.read_text()
    first, second = text.split("kAiRvas2020")
    return [int(x, 16) for x in re.findall(r"0x([0-9A-F]{8})", second if use_2020 else first)]


def main() -> None:
    args = [a for a in sys.argv[1:] if not a.startswith("--")]
    pe = pefile.PE(args[0], fast_load=True)
    base = pe.OPTIONAL_HEADER.ImageBase
    rvas = table("--2020" in sys.argv)
    md = capstone.Cs(capstone.CS_ARCH_X86, capstone.CS_MODE_32)
    for name in args[1:]:
        g = int(name.removeprefix("DAT_"), 16)
        idx = (g - 0x10066BC8) // 4
        rva = rvas[idx]
        sec = next((s for s in pe.sections
                    if s.VirtualAddress <= rva < s.VirtualAddress + s.Misc_VirtualSize), None)
        sname = sec.Name.rstrip(b"\0").decode() if sec else "?"
        print(f"== {name} slot 0x{idx:02X} rva {rva:08X} va {base + rva:08X} [{sname}]")
        data = pe.get_data(rva, 64).ljust(64, b"\0")  # .bss tail reads as empty
        if sec is not None and sec.Characteristics & 0x20000000:  # executable
            for n, i in enumerate(md.disasm(data, base + rva)):
                print(f"  {i.address:08x}  {i.bytes.hex():<20} {i.mnemonic} {i.op_str}")
                if n >= 7:
                    break
        else:
            print("  " + " ".join(f"{v:08x}" for v in struct.unpack_from("<8I", data)))


if __name__ == "__main__":
    main()

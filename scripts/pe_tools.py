"""Shared helpers of the scripts that read a 32-bit PE: disasm.py, ai_slot.py.

Needs: pip install pefile capstone
"""
import capstone
import pefile

IMAGE_SCN_MEM_EXECUTE = 0x20000000


def open_pe(path: str) -> pefile.PE:
    return pefile.PE(path, fast_load=True)


def section_of(pe: pefile.PE, rva: int):
    """The section holding `rva`, or None."""
    return next((s for s in pe.sections
                 if s.VirtualAddress <= rva < s.VirtualAddress + s.Misc_VirtualSize), None)


def print_disasm(va: int, data: bytes, max_n: int | None = None, indent: str = "") -> None:
    """Print `data` as x86-32 code at `va`, at most `max_n` instructions."""
    md = capstone.Cs(capstone.CS_ARCH_X86, capstone.CS_MODE_32)
    for n, i in enumerate(md.disasm(data, va)):
        if max_n is not None and n >= max_n:
            break
        print(f"{indent}{i.address:08x}  {i.bytes.hex():<20} {i.mnemonic} {i.op_str}")

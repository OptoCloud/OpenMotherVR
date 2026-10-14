"""Check spec/ against the mechanical part of the clean-room spec rules.

    python -I tools/spec_lint.py [spec_dir]

Exits non-zero and lists every violation found. See CLEANROOM.md.
"""
import pathlib
import re
import sys

RULES = [
    (re.compile(r"\bFUN_[0-9a-fA-F]{6,}"), "decompiler function name"),
    (re.compile(r"\b(?:DAT|LAB|UNK|PTR|SUB)_[0-9a-fA-F]{6,}"), "decompiler symbol"),
    (re.compile(r"\b0x10[0-9a-fA-F]{6}\b"), "address in a mod binary (0x10xxxxxx)"),
    (re.compile(r"\b(?:in_ECX|in_EAX|param_\d+|local_[0-9a-f]+|iVar\d+|uVar\d+|fVar\d+|pcVar\d+)\b"),
     "decompiler variable name"),
    (re.compile(r"\bdxgi\.dll\b.*\b(?:decompil|disassembl)", re.I), "reference to mod decompilation"),
    (re.compile(r"^\s*[0-9a-f]{8}:?\s+(?:[0-9a-f]{2}\s?){1,15}\s+[a-z]{2,7}\b"), "disassembly listing"),
    (re.compile(r"(?:ff 25|e8|e9) [0-9a-f]{2} [0-9a-f]{2} [0-9a-f]{2} [0-9a-f]{2}\b.*(?:mod|dxgi)", re.I),
     "mod code bytes"),
]

HEADER = re.compile(r"^<!--\s*Provenance:.*?Revision:\s*r\d+.*?-->", re.S)


def lint(path: pathlib.Path) -> list[str]:
    text = path.read_text(encoding="utf-8")
    problems = []
    if not HEADER.match(text.lstrip()):
        problems.append(f"{path}: missing provenance header (<!-- Provenance: ... Revision: rN -->)")
    for line_no, line in enumerate(text.splitlines(), 1):
        for pattern, what in RULES:
            if pattern.search(line):
                problems.append(f"{path}:{line_no}: {what}: {line.strip()[:100]}")
    return problems


def main() -> None:
    root = pathlib.Path(sys.argv[1] if len(sys.argv) > 1 else "spec")
    problems = [p for f in sorted(root.rglob("*.md")) for p in lint(f)]
    for p in problems:
        print(p)
    print(f"{len(problems)} problem(s) in {root}")
    sys.exit(1 if problems else 0)


if __name__ == "__main__":
    main()

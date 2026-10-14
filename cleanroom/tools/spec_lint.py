"""Check spec/ against the mechanical part of the clean-room spec rules.

    python -I tools/spec_lint.py [spec_dir]

Exits non-zero and lists every violation found; warnings are printed but do
not fail the run. See CLEANROOM.md.
"""
import pathlib
import re
import sys

# Module names of the kind a project-internal namespace would use. A name
# qualified by one of these (`hook::x`) is an implementation identifier, not
# a game or public API fact. Public API qualifiers (`vr::`, `std::`,
# `IDXGISwapChain::`) are not in the list.
_MODULES = "hook|diag|runtime|stereo|input|proxy|config|render|integration|game|audio|ui|util"

RULES = [
    (re.compile(r"\bFUN_[0-9a-fA-F]{6,}"), "decompiler function name"),
    (re.compile(r"\b(?:DAT|LAB|UNK|PTR|SUB)_[0-9a-fA-F]{6,}"), "decompiler symbol"),
    # A mod binary loads at 0x10000000; round constants (flags, sizes) that
    # end in five zero digits are not addresses.
    (re.compile(r"\b0x10(?![0-9a-fA-F]0{5}\b)[0-9a-fA-F]{6}\b"), "address in a mod binary (0x10xxxxxx)"),
    (re.compile(r"\b(?:in_ECX|in_EAX|param_\d+|local_[0-9a-f]+|iVar\d+|uVar\d+|fVar\d+|pcVar\d+)\b"),
     "decompiler variable name"),
    (re.compile(r"\b(?:[a-z]{1,4}Var\d+|in_[A-Z]{2,3}\b|in_stack_\w+|extraout_\w+|unaff_\w+"
                r"|auStack_\w+|local_res\w*|CONCAT\d\d|thunk_FUN_)"),
     "decompiler variable name"),
    (re.compile(r"\bdxgi\.dll\b.*\b(?:decompil|disassembl)", re.I), "reference to mod decompilation"),
    (re.compile(r"^\s*[0-9a-f]{8}:?\s+(?:[0-9a-f]{2}\s?){1,15}\s+[a-z]{2,7}\b"), "disassembly listing"),
    (re.compile(r"\b(?:ff 25|e8|e9)(?: [0-9a-f]{2}){4}\b.*\b(?:mod's|mod|dxgi\.dll)(?![A-Za-z0-9_])", re.I),
     "mod code bytes"),
    (re.compile(r"\b(?:g_[a-z]\w*|k[A-Z]\w*Rvas\w*|setMother\w*|addMother\w*|hook_installer|script_bridge"
                r"|game_hooks|recon_addresses|ai_address_tables|install_\w+"
                r"|(?:" + _MODULES + r")::\w+)"),
     "implementation identifier"),
    (re.compile(r"\b(?:current|project's) (?:rebuild|reconstruction)\b|\bthe current build\b"
                r"|\bthe reconstruction\b", re.I),
     "cites the reconstruction as a source"),
]

# Rules that do not apply inside the provenance header.
BODY_RULES = [
    (re.compile(r"\b(?:the )?(?:reference|original) mod\b", re.I), "refers to another mod"),
]

WARNINGS = [
    (re.compile(r"\bthe original's\b", re.I), "check: \"the original's\" may refer to another mod"),
]

# Other mods and their authors are never named in the spec. The project's
# own name (OpenMotherVR) is not matched: the look-behind needs a non-letter.
MOD_NAMES = re.compile(r"(?<![A-Za-z0-9_])(?:mother\s?vr|nibre|grand|vorpx|jayp)(?![A-Za-z0-9])", re.I)
# GRAND is matched case-sensitively only; "grand" is an English word.
_CASE_SENSITIVE = {"grand"}

# Compatibility names players or their files depend on. They are allowed
# only inside a code span or a fenced code block; any other mention of a mod
# name is reported.
COMPAT_TOKENS = re.compile(
    r"mothervr\.json|mothervr\.log|mothervr_input(?![A-Za-z0-9_])|MOTHER\.PAK(?:\.src)?"
    r"|(?<![A-Za-z0-9])mvr_\w*")
PAGE_TITLE_SPAN = "`MotherVR`"
CODE_SPAN = re.compile(r"`[^`]*`")

HEADER = re.compile(r"^<!--\s*Provenance:.*?Revision:\s*r\d+.*?-->", re.S)


def _names_mod(line: str, in_fence: bool) -> bool:
    if in_fence:
        line = COMPAT_TOKENS.sub("", line)

    def strip_span(m: re.Match) -> str:
        span = m.group(0)
        if span == PAGE_TITLE_SPAN:
            return "``"
        return COMPAT_TOKENS.sub("", span)

    for m in MOD_NAMES.finditer(CODE_SPAN.sub(strip_span, line)):
        if m.group(0).lower() in _CASE_SENSITIVE and m.group(0) != "GRAND":
            continue
        return True
    return False


def lint_text(text: str, name: str = "<text>") -> tuple[list[str], list[str]]:
    problems, warnings = [], []
    stripped = text.lstrip()
    header = HEADER.match(stripped)
    if not header:
        problems.append(f"{name}: missing provenance header (<!-- Provenance: ... Revision: rN -->)")
    header_lines = 0
    if header:
        lead = text[: len(text) - len(stripped)]
        header_lines = (lead + header.group(0)).count("\n") + 1
    in_fence = False
    for line_no, line in enumerate(text.splitlines(), 1):
        if line.lstrip().startswith("```"):
            in_fence = not in_fence
            continue
        rules = RULES if line_no <= header_lines else RULES + BODY_RULES
        for pattern, what in rules:
            if pattern.search(line):
                problems.append(f"{name}:{line_no}: {what}: {line.strip()[:100]}")
        if _names_mod(line, in_fence):
            problems.append(f"{name}:{line_no}: names another mod: {line.strip()[:100]}")
        for pattern, what in WARNINGS:
            if pattern.search(line):
                warnings.append(f"{name}:{line_no}: warning: {what}: {line.strip()[:100]}")
    return problems, warnings


def lint(path: pathlib.Path) -> list[str]:
    problems, _ = lint_text(path.read_text(encoding="utf-8"), str(path))
    return problems


def main() -> None:
    root = pathlib.Path(sys.argv[1] if len(sys.argv) > 1 else "spec")
    problems, warnings = [], []
    for f in sorted(root.rglob("*.md")):
        p, w = lint_text(f.read_text(encoding="utf-8"), str(f))
        problems += p
        warnings += w
    for p in problems + warnings:
        print(p)
    print(f"{len(problems)} problem(s), {len(warnings)} warning(s) in {root}")
    sys.exit(1 if problems else 0)


if __name__ == "__main__":
    main()

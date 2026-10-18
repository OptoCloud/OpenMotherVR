"""Add the v2 options pages (Comfort, Controls, Hands, Body, Interaction,
HUD, Graphics) to the menu movies of a modded UI archive, giving the archive
scripts/ui_patch.py makes its patch from.

    python -I scripts/ui_pages.py <MotherVR 0.8.1 MOTHER.PAK> <out.PAK> <work dir>
        [--ffdec <path to ffdec.jar>]

For each of NEWFRONTENDMENU, PAUSEMENU and GAMEOVERMENU it decompiles
frontend.GameMenu with JPEXS FFDec (Java on PATH), splices in
scripts/ui_pages/v2_pages.as (the pages, their handlers and the setMother*
functions the DLL pushes to), calls the page builder at the end of
setVisibility and the greying from setMotherVROptionsEnabled, recompiles the
class into the movie, and checks the result by decompiling it again. Every
other entry is copied unchanged. The work directory gets the intermediate
files; it must be outside the repository's tracked files (build-*/ is
ignored). Nothing here is shipped: the output archive goes to ui_patch.py
make, and only the patch is committed.
"""
import argparse
import re
import subprocess
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
import pak2  # noqa: E402
import ui_patch  # noqa: E402

MOVIES = ["DATA/UI/NEWFRONTENDMENU.GFX", "DATA/UI/PAUSEMENU.GFX", "DATA/UI/GAMEOVERMENU.GFX"]
CLASS = "frontend.GameMenu"
DEFAULT_FFDEC = Path(r"C:\Tools\ffdec_26.3.0\ffdec.jar")
MEMBERS = Path(__file__).resolve().parent / "ui_pages" / "v2_pages.as"

# Imports the members use that the stock class does not import.
IMPORTS = [
    "flash.events.TimerEvent",
    "flash.utils.Dictionary",
    "flash.utils.Timer",
    "frontend.menu.MenuPage",
]
# (method, statement appended at the end of its body)
CALLS = [
    ("setVisibility", "this.motherV2Build();"),
    ("setMotherVROptionsEnabled", "this.motherV2SetRunning(param1);"),
]
# Names the recompiled class must define.
REQUIRED = ["motherV2Build", "motherV2SetRunning", "setMotherSmoothTurnSpeed", "setMotherRealCrouch",
            "setMotherWorldScaleSwitch", "setMotherHudPitch", "setMotherFilmGrain"]


def ffdec(jar: Path, *args: str) -> None:
    subprocess.run(["java", "-Xmx2g", "-jar", str(jar), *args], check=True, stdout=subprocess.DEVNULL)


def add_imports(src: str) -> str:
    anchor = "   import flash.external.ExternalInterface;\n"
    if src.count(anchor) != 1:
        raise SystemExit("GameMenu: import anchor not found")
    extra = "".join(f"   import {name};\n" for name in IMPORTS if f"import {name};" not in src)
    return src.replace(anchor, anchor + extra)


def append_to_method(src: str, method: str, statement: str) -> str:
    m = re.search(rf"function {method}\([^)]*\) : void\s*\n(\s*)\{{\n", src)
    if not m or len(re.findall(rf"function {method}\(", src)) != 1:
        raise SystemExit(f"GameMenu: {method} not found once")
    depth, i = 1, m.end()
    while depth:
        depth += {"{": 1, "}": -1}.get(src[i], 0)
        i += 1
    close = src.rfind("\n", 0, i - 1) + 1  # start of the closing brace's line
    return src[:close] + m.group(1) + "   " + statement + "\n" + src[close:]


def add_members(src: str, members: str) -> str:
    end = src.rstrip().rfind("}", 0, src.rstrip().rfind("}"))  # the class's closing brace
    if end < 0:
        raise SystemExit("GameMenu: class end not found")
    line = src.rfind("\n", 0, end) + 1
    return src[:line] + "\n" + members.rstrip() + "\n" + src[line:]


def splice(src: str, members: str) -> str:
    if "motherV2Build" in src:
        raise SystemExit("GameMenu: already has the v2 pages")
    src = add_imports(src)
    for method, statement in CALLS:
        src = append_to_method(src, method, statement)
    return add_members(src, members)


def class_source(jar: Path, movie: Path, out: Path) -> str:
    ffdec(jar, "-selectclass", CLASS, "-export", "script", str(out), str(movie))
    return (out / "scripts" / "frontend" / "GameMenu.as").read_text(encoding="utf-8")


def rebuild_movie(jar: Path, name: str, data: bytes, work: Path, members: str) -> bytes:
    stem = Path(name).stem
    movie_dir = work / stem
    movie_dir.mkdir(parents=True, exist_ok=True)
    original = movie_dir / f"{stem}.GFX"
    original.write_bytes(data)
    src = splice(class_source(jar, original, movie_dir / "original"), members)
    scripts = movie_dir / "edited" / "scripts" / "frontend"
    scripts.mkdir(parents=True, exist_ok=True)
    (scripts / "GameMenu.as").write_text(src, encoding="utf-8", newline="\n")
    rebuilt = movie_dir / f"{stem}.v2.GFX"
    ffdec(jar, "-importScript", str(original), str(rebuilt), str(movie_dir / "edited" / "scripts"))
    out = rebuilt.read_bytes()
    if out[:3] != b"GFX":
        raise SystemExit(f"{name}: FFDec did not write a GFX movie")
    check = class_source(jar, rebuilt, movie_dir / "check")
    missing = [n for n in REQUIRED if f"function {n}(" not in check]
    if missing:
        raise SystemExit(f"{name}: the recompiled class lacks {', '.join(missing)}")
    print(f"{name}: {len(data)} -> {len(out)} bytes")
    return out


def main() -> None:
    ap = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    ap.add_argument("modded", type=Path)
    ap.add_argument("out", type=Path)
    ap.add_argument("work", type=Path)
    ap.add_argument("--ffdec", type=Path, default=DEFAULT_FFDEC)
    a = ap.parse_args()
    members = MEMBERS.read_text(encoding="utf-8")
    entries = pak2.read_pak(str(a.modded))
    for name in MOVIES:
        entries[name] = rebuild_movie(a.ffdec, name, entries[name], a.work, members)
    ui_patch.write_pak(a.out, ui_patch.pak_alignment(a.modded), entries)
    print(f"{a.out}: {len(entries)} entries")


if __name__ == "__main__":
    main()

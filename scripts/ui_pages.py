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
import re
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
import ffdec_movie  # noqa: E402
import pak2  # noqa: E402

MOVIES = ["DATA/UI/NEWFRONTENDMENU.GFX", "DATA/UI/PAUSEMENU.GFX", "DATA/UI/GAMEOVERMENU.GFX"]
CLASS = "frontend.GameMenu"
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
    close = ffdec_movie.line_start(src, ffdec_movie.block_end(src, m.end()) - 1)  # the closing brace's line
    return src[:close] + m.group(1) + "   " + statement + "\n" + src[close:]


def splice(src: str, members: str) -> str:
    if "motherV2Build" in src:
        raise SystemExit("GameMenu: already has the v2 pages")
    src = add_imports(src)
    for method, statement in CALLS:
        src = append_to_method(src, method, statement)
    return ffdec_movie.add_before_class_end(src, members, "GameMenu")


def lacks(check: str) -> str | None:
    missing = [n for n in REQUIRED if f"function {n}(" not in check]
    return ", ".join(missing) if missing else None


def main() -> None:
    a = ffdec_movie.parse_args(__doc__)
    members = MEMBERS.read_text(encoding="utf-8")
    entries = pak2.read_pak(str(a.modded))
    for name in MOVIES:
        entries[name] = ffdec_movie.rebuild_movie(a.ffdec, CLASS, name, entries[name], a.work, "v2",
                                                  lambda src: splice(src, members), lacks)
    pak2.write_pak(a.out, pak2.pak_alignment(a.modded), entries)
    print(f"{a.out}: {len(entries)} entries")


if __name__ == "__main__":
    main()

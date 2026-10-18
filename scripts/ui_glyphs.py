# OpenMotherVR-Provenance: clean
"""Add the mod's VR button glyphs to a modded UI archive, giving the archive
scripts/ui_patch.py makes its patch from.

    python -I scripts/ui_glyphs.py <in.PAK> <out.PAK> <work dir>
        [--ffdec <path to ffdec.jar>]

Run after scripts/ui_pages.py, on its output (spec 14 §2.9, §3.4; 22
§10.2):

- adds one image per glyph id of src/core/input/glyph_ids.h, drawn by
  scripts/glyph_art.py, as DATA/UI/GLYPH_<ID>.DDS;
- replaces the bytes of every other VR glyph image the input archive adds to
  the stock set (DATA/UI/GLYPH_VR_*.DDS) with this project's `none` glyph, so
  the patch carries no image this project did not draw (no prompt names
  those ids any more);
- in each of the 19 movies with the stock ImageSubstitution class, adds the
  method omvrLoadGlyphs, which registers every glyph with the stock
  load_image_file(file, inputId) on the "PC" and "360" platforms, and calls
  it at the end of load_image_files. The stock loader counts these images
  with its own, so on_all_images_loaded still runs once, after the last.

The class is decompiled and recompiled with JPEXS FFDec 26.3.0 (Java on
PATH), as ui_pages.py does, and checked by decompiling it again. Every
other entry is copied unchanged and the output keeps the names sorted.
The work directory must be outside the repository's tracked files (build-*/
is ignored). Nothing here is shipped: only the patch is committed.
"""
import re
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
import ffdec_movie  # noqa: E402
import glyph_art  # noqa: E402
import pak2  # noqa: E402

# The movies with ImageSubstitution (spec 14 §2.7.1).
MOVIES = [f"DATA/UI/{m}.GFX" for m in (
    "CHALLENGELEADERBOARDCOMPONENT", "CHALLENGEMAPOVERLAY", "CRAFTINGSCREEN", "GAMEOVERMENU", "GAMMASCREEN",
    "HUB", "HUB_INVENTORY", "HUB_JOURNAL", "HUB_LOGS", "HUB_MAP", "HUB_MISSING", "NEWFRONTENDMENU",
    "PAUSEMENU", "PICKUPOVERLAY", "RADIALMENU", "REACTION_GAME", "TERMINALINTERACTION", "TRCSYSTEMOVERLAY",
    "WEAPONSTUFF")]
CLASS = "ImageSubstitution"
METHOD = "omvrLoadGlyphs"
OTHER_VR_GLYPH = re.compile(r"DATA/UI/GLYPH_VR_[A-Z0-9_]+\.DDS")


def members(ids: list[str]) -> str:
    loads = "\n".join(f'         this.load_image_file("{glyph_art.file_stem(i)}","{i.lower()}");' for i in ids)
    return f"""      // OpenMotherVR: the VR button glyphs (spec 14 §2.9, §3.4; 22 §10.2),
      // ids in lower case as the stock ones. Spliced by scripts/ui_glyphs.py.
      public function {METHOD}(param1:String) : void
      {{
         if(param1 != PLATFORM_PC && param1 != PLATFORM_360)
         {{
            return;
         }}
{loads}
      }}
"""


def method_body(src: str, name: str) -> tuple[int, int, str]:
    """(start of the opening line's indent, index after the closing brace,
    the body) of the one method `name` returning void."""
    found = list(re.finditer(rf"function {name}\([^)]*\) : void\s*\n(\s*)\{{\n", src))
    if len(found) != 1:
        raise SystemExit(f"{CLASS}: {name} not found once")
    m = found[0]
    i = ffdec_movie.block_end(src, m.end())
    return m.start(1), i, src[m.end():i - 1]


def splice(src: str, ids: list[str]) -> str:
    if METHOD in src:
        raise SystemExit(f"{CLASS}: already has {METHOD}")
    for const in ("PLATFORM_PC:String", "PLATFORM_360:String"):
        if const not in src:
            raise SystemExit(f"{CLASS}: {const} not found")
    if not re.search(r"public function load_image_file\(\w+:String, \w+:String\) : void", src):
        raise SystemExit(f"{CLASS}: load_image_file(String, String) not found")
    # Some movies keep the parameter names, others have param1.
    platform = re.search(r"function load_image_files\((\w+):String\) : void", src)
    if not platform:
        raise SystemExit(f"{CLASS}: load_image_files(String) not found")
    indent_at, end, body = method_body(src, "load_image_files")
    # The call goes last; a return before it would skip it.
    if re.search(r"\breturn\b", body):
        raise SystemExit(f"{CLASS}: load_image_files returns early; the glyph call would be skipped")
    indent = src[indent_at:src.index("{", indent_at)]
    close = ffdec_movie.line_start(src, end - 1)  # the closing brace's line
    src = src[:close] + indent + f"   this.{METHOD}(param1);\n" + src[close:]
    return ffdec_movie.add_before_class_end(src, members(ids), CLASS)


def lacks(check: str, ids: list[str]) -> str | None:
    _, _, body = method_body(check, "load_image_files")
    if not re.search(rf"this\.{METHOD}\(\w+\);", body) or len(re.findall(r'load_image_file\("glyph_omvr_', check)) != len(ids):
        return "the glyph registration"
    return None


def main() -> None:
    a = ffdec_movie.parse_args(__doc__)
    ids = glyph_art.glyph_ids()
    entries = pak2.read_pak(str(a.modded))
    glyphs = glyph_art.all_glyphs()
    for name in MOVIES:
        if name not in entries:
            raise SystemExit(f"{a.modded}: no {name}")
        entries[name] = ffdec_movie.rebuild_movie(a.ffdec, CLASS, name, entries[name], a.work, "glyphs",
                                                  lambda src: splice(src, ids), lambda check: lacks(check, ids))
    none = glyphs["OMVR_NONE"]
    replaced = [n for n in entries if OTHER_VR_GLYPH.fullmatch(n)]
    for n in replaced:
        entries[n] = none
    for gid, data in glyphs.items():
        name = glyph_art.entry_name(gid)
        if name in entries:
            raise SystemExit(f"{a.modded}: already has {name}")
        entries[name] = data
    out = {n: entries[n] for n in sorted(entries)}
    pak2.write_pak(a.out, pak2.pak_alignment(a.modded), out)
    print(f"{a.out}: {len(out)} entries ({len(glyphs)} glyphs added, {len(replaced)} other glyphs replaced)")


if __name__ == "__main__":
    main()

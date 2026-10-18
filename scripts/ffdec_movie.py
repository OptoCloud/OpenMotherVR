# OpenMotherVR-Provenance: clean
"""Shared parts of the scripts that splice ActionScript into a Scaleform
movie with JPEXS FFDec 26.3.0 (Java on PATH): ui_pages.py and ui_glyphs.py.

A class is decompiled, edited as text, recompiled into the movie, and
checked by decompiling it again.
"""
import argparse
import subprocess
from collections.abc import Callable
from pathlib import Path

DEFAULT_FFDEC = Path(r"C:\Tools\ffdec_26.3.0\ffdec.jar")


def ffdec(jar: Path, *args: str) -> None:
    subprocess.run(["java", "-Xmx2g", "-jar", str(jar), *args], check=True, stdout=subprocess.DEVNULL)


def block_end(src: str, i: int) -> int:
    """The index after the brace closing the block whose body starts at i."""
    depth = 1
    while depth:
        depth += {"{": 1, "}": -1}.get(src[i], 0)
        i += 1
    return i


def line_start(src: str, i: int) -> int:
    """The start of the line holding src[i]."""
    return src.rfind("\n", 0, i) + 1


def add_before_class_end(src: str, members: str, cls: str) -> str:
    """`members` on their own lines before the class's closing brace (the
    last brace but one: the package's closes the file)."""
    end = src.rstrip().rfind("}", 0, src.rstrip().rfind("}"))
    if end < 0:
        raise SystemExit(f"{cls}: class end not found")
    line = line_start(src, end)
    return src[:line] + "\n" + members.rstrip() + "\n" + src[line:]


def class_file(cls: str) -> Path:
    """The path FFDec exports `cls` (package.Name) to, under scripts/."""
    return Path(*cls.split(".")).with_suffix(".as")


def class_source(jar: Path, cls: str, movie: Path, out: Path) -> str:
    ffdec(jar, "-selectclass", cls, "-export", "script", str(out), str(movie))
    path = out / "scripts" / class_file(cls)
    if not path.exists():
        raise SystemExit(f"{movie.name}: no {cls} class")
    return path.read_text(encoding="utf-8")


def rebuild_movie(jar: Path, cls: str, name: str, data: bytes, work: Path, tag: str,
                  splice: Callable[[str], str], lacks: Callable[[str], str | None]) -> bytes:
    """The movie `name` (bytes `data`) with class `cls` edited by `splice`.
    Intermediate files go to work/<stem>/; `lacks` gets the recompiled
    class and says what it is missing, or None."""
    stem = Path(name).stem
    movie_dir = work / stem
    movie_dir.mkdir(parents=True, exist_ok=True)
    original = movie_dir / f"{stem}.GFX"
    original.write_bytes(data)
    src = splice(class_source(jar, cls, original, movie_dir / "original"))
    scripts = movie_dir / "edited" / "scripts"
    edited = scripts / class_file(cls)
    edited.parent.mkdir(parents=True, exist_ok=True)
    edited.write_text(src, encoding="utf-8", newline="\n")
    rebuilt = movie_dir / f"{stem}.{tag}.GFX"
    ffdec(jar, "-importScript", str(original), str(rebuilt), str(scripts))
    out = rebuilt.read_bytes()
    if out[:3] != b"GFX":
        raise SystemExit(f"{name}: FFDec did not write a GFX movie")
    missing = lacks(class_source(jar, cls, rebuilt, movie_dir / "check"))
    if missing:
        raise SystemExit(f"{name}: the recompiled class lacks {missing}")
    print(f"{name}: {len(data)} -> {len(out)} bytes")
    return out


def parse_args(doc: str) -> argparse.Namespace:
    """<in.PAK> <out.PAK> <work dir> [--ffdec <jar>], as modded, out, work, ffdec."""
    ap = argparse.ArgumentParser(description=doc.splitlines()[0])
    ap.add_argument("modded", type=Path)
    ap.add_argument("out", type=Path)
    ap.add_argument("work", type=Path)
    ap.add_argument("--ffdec", type=Path, default=DEFAULT_FFDEC)
    return ap.parse_args()

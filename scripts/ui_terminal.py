# OpenMotherVR-Provenance: clean
"""Add the terminal text size variants (spec 23 §6.2) to a modded UI
archive, giving the archive scripts/ui_patch.py makes its patch from.

    python -I scripts/ui_terminal.py <in.PAK> <out.PAK>

Run after scripts/ui_glyphs.py, on its output. For each step t of
Interaction.TerminalTextScale other than 1.0 (1.25, 1.5, 1.75) it writes a
copy of the input's DATA/UI/TERMINALINTERACTION.GFX named
"DATA/UI/TERMINALINTERACTION.GFX|text125" (and text150, text175), right
after that entry, with these edits and nothing else (spec 23 §2.8, §6.2;
14 §3.6.1):

- the document body (DefineEditText 38, txt_description_text): font height
  and the size of its initial HTML become round(20 t); its bounds stay
  620 x 463;
- TerminalContentPage's internal const LINE_HEIGHT (20) becomes round(20 t),
  so scroll_up's range follows the new line height;
- entry titles (DefineEditText 33, txt_content_text, 23 px, one line):
  round(23 t), at most TITLE_MAX px, the field's height grown about its
  centre to hold the line; its width stays;
- folder names (DefineEditText 49, txt_foldername, 20 px, word wrap):
  round(20 t), at most FOLDER_MAX px, the field widened about its centre
  and grown downwards inside the folder tile so it still holds three lines.

The caps come from the stock movie's containers and the font's metrics (see
the constants below). Every record and the constant are checked against
their stock values first; any difference stops the build. The output is the
same bytes on every run. Nothing here is shipped: only the patch is.
"""
import argparse
import struct
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
import pak2  # noqa: E402
import ui_patch  # noqa: E402

ENTRY = "DATA/UI/TERMINALINTERACTION.GFX"
# The tags src/core/ui/ui_patch.cpp picks by Interaction.TerminalTextScale.
STEPS = [(1.25, "text125"), (1.5, "text150"), (1.75, "text175")]
SEPARATOR = "|"

CLASS_NS = "interactive_terminal_components"
CLASS = "TerminalContentPage"

# Stock records (twips): character id -> (bounds xmin, xmax, ymin, ymax,
# font height, flag bytes, font class).
STOCK = {
    38: ((-40, 12360, -40, 9219), 400, (0xEC, 0xB3), b"$JixellationReg"),
    33: ((-40, 9020, -40, 718), 460, (0x8C, 0xB1), b"$JixellationBold"),
    49: ((-40, 3660, -40, 2090), 400, (0x6C, 0xB1), b"$JixellationBold"),
}
BODY_HTML = b'<font size="20" '

# Line box of $JixellationBold (FONTS_EN.GFX DefineFont3, ascent + descent
# per em: 1.0859 + 0.2246), and the field gutter (2 px each side) and
# leading (the records' 2 px) of the stock fields.
BOLD_LINE = 1.3106
GUTTER = 4.0
LEADING = 2.0
# Title: one line inside the entry's background bar (character 32, 50.5 px
# tall); the field grows about its stock centre, which has 26 px above it
# and 24.5 px below it inside the bar, so it may be at most 49 px tall:
# (49 - 4) / 1.3106 = 34.3.
TITLE_MAX = 34
# Folder name: three lines, as the stock field holds, inside the folder
# tile (shapes 48, 51, 53: 202 x 122 px); the field keeps its top and may
# grow to the tile's bottom, 113.95 px tall: (113.95 - 4 - 2 * 2) / 3 /
# 1.3106 = 26.9. It is widened about its centre to 200 px (the tile is 202).
FOLDER_MAX = 26
FOLDER_BOUNDS = (-190, 3810, -40, 2239)  # twips: 200 x 113.95 px, centre kept


def round_half_up(x: float) -> int:
    return int(x + 0.5)


# ---- SWF tags ----------------------------------------------------------

class Tag:
    def __init__(self, code: int, body: bytes, long_form: bool, children=None, head: bytes = b""):
        self.code, self.body, self.long_form, self.children, self.head = code, body, long_form, children, head

    def encode(self) -> bytes:
        body = self.body if self.children is None else self.head + b"".join(t.encode() for t in self.children)
        if self.long_form or len(body) >= 0x3F:
            return struct.pack("<HI", self.code << 6 | 0x3F, len(body)) + body
        return struct.pack("<H", self.code << 6 | len(body)) + body


def parse_tags(data: bytes, p: int, end: int) -> list[Tag]:
    tags = []
    while p < end:
        (h,) = struct.unpack_from("<H", data, p)
        code, ln, p = h >> 6, h & 0x3F, p + 2
        long_form = ln == 0x3F
        if long_form:
            (ln,) = struct.unpack_from("<I", data, p)
            p += 4
        body = data[p:p + ln]
        if code == 39:  # DefineSprite: id, frame count, nested tags
            tags.append(Tag(code, body, long_form, parse_tags(body, 4, len(body)), body[:4]))
        else:
            tags.append(Tag(code, body, long_form))
        p += ln
        if code == 0:
            break
    return tags


def walk(tags: list[Tag]):
    for t in tags:
        yield t
        if t.children is not None:
            yield from walk(t.children)


def header_size(data: bytes) -> int:
    nbits = data[8] >> 3
    return 8 + (5 + 4 * nbits + 7) // 8 + 4


# ---- bit-packed RECT -----------------------------------------------------

def read_rect(data: bytes, p: int) -> tuple[tuple[int, int, int, int], int]:
    bit = p * 8

    def take(n: int) -> int:
        nonlocal bit
        v = 0
        for _ in range(n):
            v = v << 1 | (data[bit >> 3] >> (7 - (bit & 7)) & 1)
            bit += 1
        return v

    n = take(5)
    vals = []
    for _ in range(4):
        v = take(n)
        vals.append(v - (1 << n) if n and v >> (n - 1) else v)
    return tuple(vals), (bit + 7) // 8


def write_rect(vals: tuple[int, int, int, int]) -> bytes:
    n = max(max(v.bit_length() for v in vals) + 1, 1)
    bits = [(n >> (4 - i)) & 1 for i in range(5)]
    for v in vals:
        u = v & ((1 << n) - 1)
        bits += [(u >> (n - 1 - i)) & 1 for i in range(n)]
    bits += [0] * (-len(bits) % 8)
    return bytes(sum(b << (7 - i) for i, b in enumerate(bits[k:k + 8])) for k in range(0, len(bits), 8))


# ---- DefineEditText ------------------------------------------------------

def edit_text(body: bytes, char_id: int, height_px: int, bounds=None, html_size: int | None = None) -> bytes:
    (cid,) = struct.unpack_from("<H", body, 0)
    assert cid == char_id
    stock_bounds, stock_height, stock_flags, font_class = STOCK[char_id]
    rect, p = read_rect(body, 2)
    flags = (body[p], body[p + 1])
    if rect != stock_bounds or flags != stock_flags or not flags[1] & 0x80 or flags[0] & 0x01:
        raise SystemExit(f"{ENTRY}: text field {char_id} is not the stock record")
    q = p + 2
    end = body.index(b"\0", q)
    if body[q:end] != font_class:
        raise SystemExit(f"{ENTRY}: text field {char_id} has another font")
    hpos = end + 1
    (height,) = struct.unpack_from("<H", body, hpos)
    if height != stock_height:
        raise SystemExit(f"{ENTRY}: text field {char_id} font height is not the stock one")
    out = body[:2] + write_rect(bounds or stock_bounds) + body[p:hpos] + struct.pack("<H", height_px * 20)
    rest = body[hpos + 2:]
    if html_size is not None:
        if rest.count(BODY_HTML) != 1:
            raise SystemExit(f"{ENTRY}: text field {char_id} initial HTML is not the stock one")
        rest = rest.replace(BODY_HTML, f'<font size="{html_size}" '.encode())
    return out + rest


# ---- ABC: the LINE_HEIGHT constant ---------------------------------------

class Abc:
    def __init__(self, b: bytes, p: int):
        self.b, self.p = b, p

    def u8(self) -> int:
        self.p += 1
        return self.b[self.p - 1]

    def u30(self) -> int:
        v, shift = 0, 0
        while True:
            c = self.u8()
            v |= (c & 0x7F) << shift
            shift += 7
            if not c & 0x80 or shift >= 35:
                return v

    def s32(self) -> int:
        start = self.p
        v = self.u30()
        n = (self.p - start) * 7
        return v - (1 << n) if n < 32 and v >> (n - 1) & 1 else (v - (1 << 32) if v >> 31 else v)


def enc_u30(v: int) -> bytes:
    out = bytearray()
    v &= 0xFFFFFFFF
    while True:
        c = v & 0x7F
        v >>= 7
        if v:
            out.append(c | 0x80)
        else:
            out.append(c)
            return bytes(out)


def skip_traits(r: Abc, want=None) -> dict:
    found = {}
    for _ in range(r.u30()):
        name = r.u30()
        kind = r.u8()
        t = kind & 0x0F
        if t in (0, 6):
            r.u30()
            type_name = r.u30()
            vpos = r.p
            vindex = r.u30()
            vend = r.p
            vkind = r.u8() if vindex else None
            if want is not None and name in want:
                found[name] = (t, type_name, vindex, vkind, vpos, vend)
        else:
            r.u30()
            r.u30()
        if kind & 0x40:
            for _ in range(r.u30()):
                r.u30()
    return found


def edit_line_height(abc: bytes, start: int, new_value: int) -> bytes:
    """Sets TerminalContentPage's LINE_HEIGHT (an int const, stock 20) to
    `new_value` in the ABC file at abc[start:], reusing the int pool entry of
    that value or appending one."""
    r = Abc(abc, start + 4)  # minor, major
    count_pos = r.p
    n_int = r.u30()
    count_end = r.p
    ints = [0] + [r.s32() for _ in range(max(n_int - 1, 0))]
    ints_end = r.p
    for _ in range(max(r.u30() - 1, 0)):
        r.u30()
    n_double = r.u30()
    r.p += 8 * max(n_double - 1, 0)
    strings = [""]
    for _ in range(max(r.u30() - 1, 0)):
        n = r.u30()
        strings.append(abc[r.p:r.p + n].decode("utf-8"))
        r.p += n
    namespaces = [(0, 0)]
    for _ in range(max(r.u30() - 1, 0)):
        namespaces.append((r.u8(), r.u30()))
    for _ in range(max(r.u30() - 1, 0)):
        for _ in range(r.u30()):
            r.u30()
    multinames = [None]
    for _ in range(max(r.u30() - 1, 0)):
        k = r.u8()
        if k in (0x07, 0x0D):
            multinames.append((r.u30(), r.u30()))
        elif k in (0x0F, 0x10):
            multinames.append((None, r.u30()))
        elif k in (0x11, 0x12):
            multinames.append(None)
        elif k in (0x09, 0x0E):
            name = r.u30()
            r.u30()
            multinames.append((None, name))
        elif k in (0x1B, 0x1C):
            r.u30()
            multinames.append(None)
        elif k == 0x1D:
            qn = r.u30()
            for _ in range(r.u30()):
                r.u30()
            multinames.append(multinames[qn])
        else:
            raise SystemExit(f"{ENTRY}: unknown multiname kind {k:#x}")
    for _ in range(r.u30()):  # methods
        pc = r.u30()
        r.u30()
        for _ in range(pc):
            r.u30()
        r.u30()
        flags = r.u8()
        if flags & 0x08:
            for _ in range(r.u30()):
                r.u30()
                r.u8()
        if flags & 0x80:
            for _ in range(pc):
                r.u30()
    for _ in range(r.u30()):  # metadata
        r.u30()
        for _ in range(2 * r.u30()):
            r.u30()

    def name_of(mn: int) -> str:
        return strings[multinames[mn][1]] if multinames[mn] else ""

    def ns_of(mn: int) -> str:
        ns = multinames[mn][0] if multinames[mn] else None
        return strings[namespaces[ns][1]] if ns else ""

    targets = {i for i in range(1, len(multinames)) if name_of(i) in ("LINE_HEIGHT", "TEXT_PAGE_HEIGHT")}
    hits = []
    for _ in range(r.u30()):  # instances
        cls = r.u30()
        r.u30()
        flags = r.u8()
        if flags & 0x08:
            r.u30()
        for _ in range(r.u30()):
            r.u30()
        r.u30()
        found = skip_traits(r, targets)
        if name_of(cls) == CLASS and ns_of(cls) == CLASS_NS:
            hits.append({name_of(k): v for k, v in found.items()})
    if len(hits) != 1:
        raise SystemExit(f"{ENTRY}: {CLASS} not found once")
    consts = hits[0]
    int_type = [i for i in range(1, len(multinames)) if name_of(i) == "int" and ns_of(i) == ""]
    for name, value in (("LINE_HEIGHT", 20), ("TEXT_PAGE_HEIGHT", 10)):
        t = consts.get(name)
        if not t or t[0] != 6 or t[1] not in int_type or t[3] != 0x03 or ints[t[2]] != value:
            raise SystemExit(f"{ENTRY}: {CLASS}.{name} is not the stock int const {value}")
    _, _, _, _, vpos, vend = consts["LINE_HEIGHT"]
    if new_value in ints[1:]:
        index = ints.index(new_value, 1)
        return abc[:vpos] + enc_u30(index) + abc[vend:]
    index = len(ints)
    return (abc[:count_pos] + enc_u30(n_int + 1 if n_int else 2) + abc[count_end:ints_end] + enc_u30(new_value)
            + abc[ints_end:vpos] + enc_u30(index) + abc[vend:])


def edit_abc_tag(body: bytes, line_height: int) -> bytes:
    name_end = body.index(b"\0", 4)
    return body[:name_end + 1] + edit_line_height(body[name_end + 1:], 0, line_height)


# ---- the variant -----------------------------------------------------------

def variant(movie: bytes, t: float) -> bytes:
    if movie[:3] != b"GFX":
        raise SystemExit(f"{ENTRY}: not a GFX movie")
    head = header_size(movie)
    tags = parse_tags(movie, head, len(movie))
    body_h = round_half_up(20 * t)
    title_h = min(round_half_up(23 * t), TITLE_MAX)
    folder_h = min(round_half_up(20 * t), FOLDER_MAX)
    edited = {38: 0, 33: 0, 49: 0}
    abc_edits = 0
    for tag in walk(tags):
        if tag.code == 37:
            (cid,) = struct.unpack_from("<H", tag.body, 0)
            if cid == 38:
                tag.body = edit_text(tag.body, 38, body_h, html_size=body_h)
            elif cid == 33:
                xmin, xmax, ymin, ymax = STOCK[33][0]
                need = round_half_up((BOLD_LINE * title_h + GUTTER) * 20)
                grow = max(need - (ymax - ymin), 0)
                bounds = (xmin, xmax, ymin - grow // 2, ymax + (grow - grow // 2))
                tag.body = edit_text(tag.body, 33, title_h, bounds)
            elif cid == 49:
                tag.body = edit_text(tag.body, 49, folder_h, FOLDER_BOUNDS)
            else:
                continue
            edited[cid] += 1
        elif tag.code == 82:
            try:
                tag.body = edit_abc_tag(tag.body, body_h)
                abc_edits += 1
            except SystemExit as e:
                if "not found once" not in str(e):
                    raise
    if any(n != 1 for n in edited.values()) or abc_edits != 1:
        raise SystemExit(f"{ENTRY}: expected each text field and {CLASS} once")
    out = bytearray(movie[:head] + b"".join(tag.encode() for tag in tags))
    struct.pack_into("<I", out, 4, len(out))
    return bytes(out)


def main() -> None:
    ap = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    ap.add_argument("modded", type=Path)
    ap.add_argument("out", type=Path)
    a = ap.parse_args()
    entries = pak2.read_pak(str(a.modded))
    if ENTRY not in entries:
        raise SystemExit(f"{a.modded}: no {ENTRY}")
    if any(SEPARATOR in n for n in entries):
        raise SystemExit(f"{a.modded}: already has variant entries")
    out = {}
    for name, data in entries.items():
        out[name] = data
        if name == ENTRY:
            for t, tag in STEPS:
                out[f"{name}{SEPARATOR}{tag}"] = variant(data, t)
                print(f"{name}{SEPARATOR}{tag}: {len(data)} -> {len(out[name + SEPARATOR + tag])} bytes")
    ui_patch.write_pak(a.out, ui_patch.pak_alignment(a.modded), out)
    print(f"{a.out}: {len(out)} entries")


if __name__ == "__main__":
    main()

# OpenMotherVR-Provenance: clean
"""Draw the mod's VR button glyphs and encode them as the game's glyph DDS.

    python -I scripts/glyph_art.py <out dir> [--png]

Writes GLYPH_<ID>.DDS for every glyph id in src/core/input/glyph_ids.h (and
a PNG preview of each with --png). The designs are spec 04 §4.1 and 22
§10.2, drawn here from simple shapes (rings, discs, wedges, rounded tabs,
triangles and letters built from strokes), so no font, image or other
asset is read: the output depends only on this file and numpy.

Format (spec 14 §2.9): 32 x 32, BC3 (DXT5), one level, the stock header
fields. Drawing: every pixel is sampled 8 x 8 times; each shape is a test of
the sample points, later shapes over earlier ones; a pixel's alpha is the
covered share of its samples and its grey the mean grey of those samples.
The BC3 encoder below is this project's own and deterministic.
"""
import argparse
import re
import struct
from pathlib import Path

import numpy as np

SIZE = 32
SS = 8  # samples per pixel along each axis
N = SIZE * SS

WHITE = 224  # round glyphs: near-white (04 §4.1)
GREY = 192   # grip, stick and bumper shapes: light grey
MARK_GREY = 160  # the small direction triangles: a darker grey
BLACK = 0

IDS_HEADER = Path(__file__).resolve().parent.parent / "src" / "core" / "input" / "glyph_ids.h"


def glyph_ids() -> list[str]:
    """The ids the DLL returns, in the order glyph_ids.h lists them."""
    text = IDS_HEADER.read_text(encoding="utf-8")
    ids = re.findall(r'inline constexpr const char\* kGlyph\w+\s*=\s*"(OMVR_[A-Z0-9_]+)";', text)
    if not ids or len(ids) != len(set(ids)):
        raise SystemExit(f"{IDS_HEADER}: no glyph ids, or a repeated one")
    return ids


# ---- geometry on the sample grid ---------------------------------------------

class Canvas:
    """Sample coordinates in pixels (0..32, y down) and the drawn layers."""

    def __init__(self, x: np.ndarray, y: np.ndarray):
        self.x, self.y = x, y
        self.grey = np.zeros(x.shape, dtype=np.float64)
        self.cover = np.zeros(x.shape, dtype=bool)

    def fill(self, mask: np.ndarray, grey: int) -> None:
        self.grey[mask] = grey
        self.cover |= mask

    def erase(self, mask: np.ndarray) -> None:
        self.cover &= ~mask

    # Shapes: each returns the mask of samples inside it.
    def disc(self, cx, cy, r):
        return (self.x - cx) ** 2 + (self.y - cy) ** 2 <= r * r

    def ring(self, cx, cy, r, width):
        d = np.hypot(self.x - cx, self.y - cy)
        return np.abs(d - r) <= width / 2

    def rect(self, x0, y0, x1, y1):
        return (self.x >= x0) & (self.x <= x1) & (self.y >= y0) & (self.y <= y1)

    def round_rect(self, x0, y0, x1, y1, r, left=True, right=True):
        """A rectangle whose left and/or right corners are rounded by r."""
        inside = self.rect(x0, y0, x1, y1)
        corners = []
        if left:
            corners += [(x0 + r, y0 + r, -1, -1), (x0 + r, y1 - r, -1, 1)]
        if right:
            corners += [(x1 - r, y0 + r, 1, -1), (x1 - r, y1 - r, 1, 1)]
        for cx, cy, sx, sy in corners:
            outside_x = (self.x - cx) * sx > 0
            outside_y = (self.y - cy) * sy > 0
            inside &= ~(outside_x & outside_y & ~self.disc(cx, cy, r))
        return inside

    def ellipse_arc(self, cx, cy, rx, ry, width, a0, a1):
        """Part of an elliptical ring, between angles a0 and a1 in degrees."""
        u, v = (self.x - cx) / rx, (cy - self.y) / ry
        d = np.hypot(u, v)
        ang = np.degrees(np.arctan2(v, u)) % 360
        lo, hi = a0 % 360, a1 % 360
        within = (ang >= lo) & (ang <= hi) if lo <= hi else (ang >= lo) | (ang <= hi)
        return (np.abs(d - 1) * min(rx, ry) <= width / 2) & within

    def stroke(self, x0, y0, x1, y1, width):
        """A line with round ends."""
        px, py = self.x - x0, self.y - y0
        dx, dy = x1 - x0, y1 - y0
        t = np.clip((px * dx + py * dy) / (dx * dx + dy * dy), 0.0, 1.0)
        return np.hypot(px - t * dx, py - t * dy) <= width / 2

    def arc(self, cx, cy, r, width, a0, a1):
        """Part of a ring, between angles a0 and a1 in degrees (0 right, 90 up)."""
        ang = np.degrees(np.arctan2(cy - self.y, self.x - cx)) % 360
        lo, hi = a0 % 360, a1 % 360
        within = (ang >= lo) & (ang <= hi) if lo <= hi else (ang >= lo) | (ang <= hi)
        return self.ring(cx, cy, r, width) & within

    def wedge(self, cx, cy, r, a0, a1):
        """A filled sector of a disc, between angles a0 and a1 in degrees."""
        ang = np.degrees(np.arctan2(cy - self.y, self.x - cx)) % 360
        lo, hi = a0 % 360, a1 % 360
        within = (ang >= lo) & (ang <= hi) if lo <= hi else (ang >= lo) | (ang <= hi)
        return self.disc(cx, cy, r) & within

    def triangle(self, p0, p1, p2):
        def side(a, b):
            return (b[0] - a[0]) * (self.y - a[1]) - (b[1] - a[1]) * (self.x - a[0])
        s0, s1, s2 = side(p0, p1), side(p1, p2), side(p2, p0)
        return ((s0 >= 0) & (s1 >= 0) & (s2 >= 0)) | ((s0 <= 0) & (s1 <= 0) & (s2 <= 0))


# ---- letters, from strokes in a box (x, y top-left, h tall, w wide) -----------

def letter(c: Canvas, ch: str, x: float, y: float, h: float, w: float, sw: float):
    m = np.zeros(c.x.shape, dtype=bool)
    b = y + h  # baseline
    mid = y + h / 2
    if ch == "A":
        m |= c.stroke(x, b, x + w / 2, y, sw) | c.stroke(x + w / 2, y, x + w, b, sw)
        m |= c.stroke(x + w * 0.22, y + h * 0.66, x + w * 0.78, y + h * 0.66, sw)
    elif ch == "B":
        r = h / 4
        m |= c.stroke(x, y, x, b, sw)
        m |= c.stroke(x, y, x + w - r, y, sw) | c.stroke(x, mid, x + w - r, mid, sw)
        m |= c.stroke(x, b, x + w - r, b, sw)
        m |= c.arc(x + w - r, y + r, r, sw, -90, 90) | c.arc(x + w - r, mid + r, r, sw, -90, 90)
    elif ch == "X":
        m |= c.stroke(x, y, x + w, b, sw) | c.stroke(x + w, y, x, b, sw)
    elif ch == "Y":
        m |= c.stroke(x, y, x + w / 2, mid, sw) | c.stroke(x + w, y, x + w / 2, mid, sw)
        m |= c.stroke(x + w / 2, mid, x + w / 2, b, sw)
    elif ch == "L":
        m |= c.stroke(x, y, x, b, sw) | c.stroke(x, b, x + w, b, sw)
    elif ch == "R":
        r = h / 4
        m |= c.stroke(x, y, x, b, sw)
        m |= c.stroke(x, y, x + w - r, y, sw) | c.stroke(x, mid, x + w - r, mid, sw)
        m |= c.arc(x + w - r, y + r, r, sw, -90, 90)
        m |= c.stroke(x + w * 0.45, mid, x + w, b, sw)
    elif ch == "G":
        rx, ry = w / 2, h / 2
        cx, cy = x + rx, y + ry
        m |= c.ellipse_arc(cx, cy, rx, ry, sw, 45, 360)  # open at the upper right
        m |= c.stroke(cx + rx * 0.15, cy, cx + rx, cy, sw) | c.stroke(cx + rx, cy, cx + rx, cy + ry * 0.7, sw)
    else:
        raise SystemExit(f"no strokes for letter {ch!r}")
    return m


def text(c: Canvas, s: str, cx: float, cy: float, h: float, w: float, gap: float, sw: float):
    """Letters of one height, centred on (cx, cy)."""
    widths = [w * 1.3 if ch == "G" else w for ch in s]  # G is round, so wider
    total = sum(widths) + (len(s) - 1) * gap
    x = cx - total / 2
    m = np.zeros(c.x.shape, dtype=bool)
    for ch, cw in zip(s, widths):
        m |= letter(c, ch, x, cy - h / 2, h, cw, sw)
        x += cw + gap
    return m


# ---- the designs (04 §4.1, 22 §10.2), in a 32 x 32 box centred on (16, 16) ----

RING_R, RING_W = 13.6, 2.4    # face and menu button outline
PAD_R, PAD_W = 14.0, 1.4      # the touchpad: a thinner ring
LETTER_H, LETTER_W, LETTER_SW = 12.0, 8.5, 2.6


def face_button(ch):
    def draw(c):
        c.fill(c.ring(16, 16, RING_R, RING_W), WHITE)
        c.fill(letter(c, ch, 16 - LETTER_W / 2, 16 - LETTER_H / 2, LETTER_H, LETTER_W, LETTER_SW), WHITE)
    return draw


def menu_button(c):
    c.fill(c.ring(16, 16, RING_R, RING_W), WHITE)
    for y in (10.6, 16.0, 21.4):
        c.fill(c.round_rect(9.5, y - 1.4, 22.5, y + 1.4, 1.0), WHITE)


def grip(right):
    def draw(c):
        c.fill(c.round_rect(2.5, 7.5, 29.5, 24.5, 8.5, left=not right, right=right), GREY)
        c.fill(text(c, "RG" if right else "LG", 16, 16, 10.0, 6.4, 1.6, 2.2), BLACK)
    return draw


def left_bumper(c):
    # Thinner than a grip tab: the Frame's bumper sits above its trigger.
    c.fill(c.round_rect(2.5, 10.5, 29.5, 21.5, 5.5, left=True, right=False), GREY)
    c.fill(text(c, "LB", 16, 16, 7.5, 5.0, 1.6, 1.9), BLACK)


def right_stick_click(c):
    c.fill(c.triangle((10.5, 1.5), (21.5, 1.5), (16, 7.5)), MARK_GREY)
    c.fill(c.rect(12.5, 20.0, 19.5, 30.5), GREY)  # the stem
    c.fill(c.round_rect(5.0, 10.0, 27.0, 21.5, 4.0), GREY)  # the cap
    c.fill(letter(c, "R", 12.8, 11.8, 8.0, 6.4, 2.1), BLACK)


def left_stick(direction):
    # Seen from above, unlike the click design, so a direction is never read
    # as a click (22 §10.2).
    def draw(c):
        c.fill(c.disc(16, 16, 9.5), GREY)
        c.fill(letter(c, "L", 13.2, 12.0, 8.0, 5.6, 2.1), BLACK)
        tri = {
            "U": ((16, 0.8), (10.8, 5.6), (21.2, 5.6)),
            "D": ((16, 31.2), (10.8, 26.4), (21.2, 26.4)),
            "L": ((0.8, 16), (5.6, 10.8), (5.6, 21.2)),
            "R": ((31.2, 16), (26.4, 10.8), (26.4, 21.2)),
        }[direction]
        c.fill(c.triangle(*tri), MARK_GREY)
    return draw


def touchpad(region):
    # The quarters are the regions 04 §5.1 decodes, as wedges whose point
    # reaches the centre.
    def draw(c):
        c.fill(c.ring(16, 16, PAD_R, PAD_W), WHITE)
        if region == "C":
            c.fill(c.disc(16, 16, 3.6), WHITE)
        elif region == "A":
            c.fill(c.disc(16, 16, PAD_R), WHITE)
        else:
            a = {"R": 0, "U": 90, "L": 180, "D": 270}[region]
            c.fill(c.wedge(16, 16, PAD_R, a - 45, a + 45), WHITE)
    return draw


def trackpad(c):
    # The Index trackpad is an upright oblong; the press is anywhere on it.
    c.fill(c.round_rect(10.0, 1.5, 22.0, 30.5, 6.0), WHITE)


def view_button(c):
    c.fill(c.ring(16, 16, RING_R, RING_W), WHITE)
    back = c.round_rect(8.0, 9.0, 18.5, 17.5, 1.0) & ~c.round_rect(9.8, 10.8, 16.7, 15.7, 0.5)
    front_outer = c.round_rect(13.5, 14.5, 24.0, 23.0, 1.0)
    front = front_outer & ~c.round_rect(15.3, 16.3, 22.2, 21.2, 0.5)
    c.fill(back & ~front_outer, WHITE)
    c.fill(front, WHITE)


def unbound(c):
    # A "none" sign, unlike every bound glyph (04 §4.1).
    c.fill(c.ring(16, 16, PAD_R, PAD_W), WHITE)
    c.fill(c.stroke(25.6, 6.4, 6.4, 25.6, 1.8), WHITE)


DESIGNS = {
    "A": face_button("A"), "B": face_button("B"), "X": face_button("X"), "Y": face_button("Y"),
    "MENU": menu_button,
    "LG": grip(False), "RG": grip(True),
    "RSC": right_stick_click,
    "LS_U": left_stick("U"), "LS_D": left_stick("D"), "LS_L": left_stick("L"), "LS_R": left_stick("R"),
    "TP_C": touchpad("C"), "TP_U": touchpad("U"), "TP_D": touchpad("D"), "TP_L": touchpad("L"),
    "TP_R": touchpad("R"), "TP_A": touchpad("A"),
    "TRACKPAD": trackpad,
    "LBUMPER": left_bumper,
    "VIEW": view_button,
    "NONE": unbound,
}

# The hand mark (22 §10.2): the design scaled into the upper part of the box,
# and a short bar under its left or right half.
MARK_SCALE, MARK_CENTRE_Y = 0.80, 13.4
MARK_BAR = {"L": (4.5, 27.9, 14.5, 30.6), "R": (17.5, 27.9, 27.5, 30.6)}


def sample_grid():
    s = (np.arange(N, dtype=np.float64) + 0.5) / SS
    return np.meshgrid(s, s)  # x, y


def render(glyph_id: str) -> np.ndarray:
    """RGBA, 32 x 32, uint8."""
    name = glyph_id.removeprefix("OMVR_")
    hand = None
    if name[-2:] in ("_L", "_R") and name[:-2] in DESIGNS:
        name, hand = name[:-2], name[-1]
    if name not in DESIGNS:
        raise SystemExit(f"{glyph_id}: no design")
    x, y = sample_grid()
    if hand:
        # Sample the design in its own box: inverse of the scale about the
        # mark's centre.
        dx, dy = (x - 16) / MARK_SCALE + 16, (y - MARK_CENTRE_Y) / MARK_SCALE + 16
        c = Canvas(dx, dy)
        DESIGNS[name](c)
        bar = Canvas(x, y)
        bar.fill(bar.round_rect(*MARK_BAR[hand], 1.2), WHITE)
        c.grey[bar.cover] = WHITE
        c.cover |= bar.cover
    else:
        c = Canvas(x, y)
        DESIGNS[name](c)
    cover = c.cover.reshape(SIZE, SS, SIZE, SS).astype(np.float64)
    grey = (c.grey * c.cover).reshape(SIZE, SS, SIZE, SS)
    n = cover.sum(axis=(1, 3))
    alpha = n / (SS * SS)
    g = np.where(n > 0, grey.sum(axis=(1, 3)) / np.maximum(n, 1), 0)
    out = np.zeros((SIZE, SIZE, 4), dtype=np.uint8)
    out[..., 0] = out[..., 1] = out[..., 2] = np.rint(g).astype(np.uint8)
    out[..., 3] = np.rint(alpha * 255).astype(np.uint8)
    return out


# ---- BC3 (DXT5) ---------------------------------------------------------------

def to565(g: int) -> int:
    r5, g6 = (g * 31 + 127) // 255, (g * 63 + 127) // 255
    return (r5 << 11) | (g6 << 5) | r5


def from565(c: int) -> np.ndarray:
    r, g, b = (c >> 11) & 31, (c >> 5) & 63, c & 31
    return np.array([(r << 3) | (r >> 2), (g << 2) | (g >> 4), (b << 3) | (b >> 2)], dtype=np.int32)


def alpha_block(a: np.ndarray) -> bytes:
    a0, a1 = int(a.max()), int(a.min())
    if a0 == a1:
        return bytes([a0, a1]) + b"\0" * 6
    pal = [a0, a1] + [((7 - i) * a0 + i * a1 + 3) // 7 for i in range(1, 7)]
    bits = 0
    for i, v in enumerate(a.reshape(-1)):
        idx = min(range(8), key=lambda k: (abs(pal[k] - int(v)), k))
        bits |= idx << (3 * i)
    return bytes([a0, a1]) + bits.to_bytes(6, "little")


def colour_block(g: np.ndarray, a: np.ndarray) -> bytes:
    seen = g[a > 0]
    hi, lo = (int(seen.max()), int(seen.min())) if seen.size else (0, 0)
    c0, c1 = to565(hi), to565(lo)
    if c0 == c1:
        return struct.pack("<HHI", c0, c1, 0)
    p0, p1 = from565(c0), from565(c1)
    pal = [p0, p1, (2 * p0 + p1 + 1) // 3, (p0 + 2 * p1 + 1) // 3]
    bits = 0
    for i, v in enumerate(g.reshape(-1)):
        target = np.array([int(v)] * 3, dtype=np.int32)
        idx = min(range(4), key=lambda k: (int(((pal[k] - target) ** 2).sum()), k))
        bits |= idx << (2 * i)
    return struct.pack("<HHI", c0, c1, bits)


def dds(rgba: np.ndarray) -> bytes:
    """The stock glyph format of spec 14 §2.9."""
    header = struct.pack(
        "<4s7I44x" "2I4s5I" "4I4x",
        b"DDS ", 124, 0x00081007, SIZE, SIZE, 1024, 0, 0,
        32, 0x4, b"DXT5", 0, 0, 0, 0, 0,
        0x1000, 0, 0, 0)
    assert len(header) == 128
    body = bytearray()
    for by in range(0, SIZE, 4):
        for bx in range(0, SIZE, 4):
            blk = rgba[by:by + 4, bx:bx + 4]
            body += alpha_block(blk[..., 3]) + colour_block(blk[..., 1], blk[..., 3])
    assert len(body) == 1024
    return header + bytes(body)


def entry_name(glyph_id: str) -> str:
    """The archive entry the movie loads for an id (spec 14 §2.9)."""
    return f"DATA/UI/GLYPH_{glyph_id.upper()}.DDS"


def file_stem(glyph_id: str) -> str:
    """The file the movie asks for: load_image_file appends .dds."""
    return f"glyph_{glyph_id.lower()}"


def all_glyphs() -> dict[str, bytes]:
    return {gid: dds(render(gid)) for gid in glyph_ids()}


def main() -> None:
    ap = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    ap.add_argument("out", type=Path)
    ap.add_argument("--png", action="store_true", help="also write a PNG preview of each glyph (needs Pillow)")
    a = ap.parse_args()
    a.out.mkdir(parents=True, exist_ok=True)
    for gid in glyph_ids():
        rgba = render(gid)
        (a.out / Path(entry_name(gid)).name).write_bytes(dds(rgba))
        if a.png:
            from PIL import Image
            Image.fromarray(rgba, "RGBA").save(a.out / f"{gid}.png")
    print(f"{a.out}: {len(glyph_ids())} glyphs")


if __name__ == "__main__":
    main()

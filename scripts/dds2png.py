"""Convert a mod debug dump (.dds with a DX10 header) to .png.

    python -I scripts/dds2png.py <in.dds> [out.png] [--exposure X]

Handles the formats the game's render targets use: R8G8B8A8 / B8G8R8A8
UNORM(_SRGB), R10G10B10A2_UNORM, R11G11B10_FLOAT and R16G16B16A16_FLOAT.
Float formats are tone-mapped with x / (1 + x) after the exposure.
Needs: pip install numpy pillow
"""
import pathlib
import struct
import sys

import numpy as np
from PIL import Image


def unpack_r11g11b10(words: np.ndarray) -> np.ndarray:
    def small_float(bits: np.ndarray, mant_bits: int) -> np.ndarray:
        exp = (bits >> mant_bits) & 0x1F
        mant = bits & ((1 << mant_bits) - 1)
        val = np.where(exp == 0, mant / (1 << mant_bits) * 2.0 ** -14,
                       (1 + mant / (1 << mant_bits)) * 2.0 ** (exp.astype(np.int32) - 15))
        return val.astype(np.float32)
    r = small_float(words & 0x7FF, 6)
    g = small_float((words >> 11) & 0x7FF, 6)
    b = small_float((words >> 22) & 0x3FF, 5)
    return np.stack([r, g, b], axis=-1)


def main() -> None:
    args = [a for a in sys.argv[1:] if not a.startswith("--")]
    exposure = 1.0
    if "--exposure" in sys.argv:
        exposure = float(sys.argv[sys.argv.index("--exposure") + 1])
        args = [a for a in args if a != str(sys.argv[sys.argv.index("--exposure") + 1])]
    src = pathlib.Path(args[0])
    dst = pathlib.Path(args[1]) if len(args) > 1 else src.with_suffix(".png")
    data = src.read_bytes()
    assert data[:4] == b"DDS ", "not a DDS file"
    height, width, pitch = struct.unpack_from("<III", data, 12)
    fmt = struct.unpack_from("<I", data, 128)[0]
    pixels = data[148:148 + pitch * height]
    rows = np.frombuffer(pixels, dtype=np.uint8).reshape(height, pitch)

    if fmt in (28, 29, 87, 91):  # R8G8B8A8 / B8G8R8A8 (UNORM, SRGB)
        img = rows[:, :width * 4].reshape(height, width, 4)[..., :3]
        if fmt in (87, 91):
            img = img[..., ::-1]
        out = img
    elif fmt == 24:  # R10G10B10A2_UNORM
        w = rows[:, :width * 4].copy().view(np.uint32).reshape(height, width)
        rgb = np.stack([w & 0x3FF, (w >> 10) & 0x3FF, (w >> 20) & 0x3FF], axis=-1) / 1023.0
        out = (np.clip(rgb, 0, 1) * 255).astype(np.uint8)
    else:
        if fmt == 26:  # R11G11B10_FLOAT
            w = rows[:, :width * 4].copy().view(np.uint32).reshape(height, width)
            rgb = unpack_r11g11b10(w)
        elif fmt == 10:  # R16G16B16A16_FLOAT
            rgb = rows[:, :width * 8].copy().view(np.float16).reshape(height, width, 4)[..., :3].astype(np.float32)
        else:
            raise SystemExit(f"unsupported DXGI format {fmt}")
        rgb = np.nan_to_num(rgb) * exposure
        out = (np.clip(rgb / (1.0 + rgb), 0, 1) ** (1 / 2.2) * 255).astype(np.uint8)
    Image.fromarray(np.ascontiguousarray(out)).save(dst)
    print(f"{src.name}: {width}x{height} format {fmt} -> {dst}")


if __name__ == "__main__":
    main()

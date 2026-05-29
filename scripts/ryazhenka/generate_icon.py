#!/usr/bin/env python3
"""Procedurally generate the Ryazhenka Updater app icon.

Produces a 256x256 JPEG matching the default "Ryazhenka" palette: a warm
radial chocolate-to-cream background, a red-brown ring, a large cream "Я"
glyph with a soft shadow, and a cream sine wave along the bottom — echoing
the in-app animated background.

Usage:
    pip3 install Pillow
    python3 scripts/ryazhenka/generate_icon.py [output.jpg]

Default output: icon.jpg in the repository root (the asset the Makefile
embeds into the NRO's NACP).
"""

import math
import sys
from pathlib import Path

from PIL import Image, ImageDraw, ImageFont, ImageFilter

SIZE = 256

# Ryazhenka dark palette (matches source/ryazhenka_palette.cpp).
BG_DEEP = (26, 14, 8)      # #1A0E08 chocolate
BG_WARM = (74, 50, 37)     # #4A3225 separator-ish, for the radial edge
CREAM = (245, 230, 200)    # #F5E6C8 text
ACCENT = (212, 165, 116)   # #D4A574 cream-gold accent
REDBROWN = (232, 74, 47)   # #E84A2F highlight
RING = (139, 58, 47)       # #8B3A2F red-brown


def radial_background(img: Image.Image) -> None:
    cx = cy = SIZE / 2
    max_r = math.hypot(cx, cy)
    px = img.load()
    for y in range(SIZE):
        for x in range(SIZE):
            t = min(1.0, math.hypot(x - cx, y - cy) / max_r)
            px[x, y] = tuple(
                int(BG_DEEP[i] + (BG_WARM[i] - BG_DEEP[i]) * (t ** 1.3))
                for i in range(3)
            )


def load_font(size: int) -> ImageFont.FreeTypeFont:
    candidates = [
        "/usr/share/fonts/truetype/dejavu/DejaVuSans-Bold.ttf",
        "/usr/share/fonts/truetype/liberation/LiberationSans-Bold.ttf",
        "/usr/share/fonts/TTF/DejaVuSans-Bold.ttf",
    ]
    for c in candidates:
        if Path(c).exists():
            return ImageFont.truetype(c, size)
    return ImageFont.load_default()


def draw_wave(draw: ImageDraw.ImageDraw, color, base_y: float, amp: float,
              wavelength: float, phase: float, width: int) -> None:
    pts = []
    for x in range(0, SIZE + 1, 2):
        y = base_y + math.sin((x / wavelength) * 2 * math.pi + phase) * amp
        pts.append((x, y))
    draw.line(pts, fill=color, width=width, joint="curve")


def generate(out_path: Path) -> None:
    img = Image.new("RGB", (SIZE, SIZE), BG_DEEP)
    radial_background(img)
    draw = ImageDraw.Draw(img)

    # Outer red-brown ring.
    margin = 14
    draw.ellipse([margin, margin, SIZE - margin, SIZE - margin],
                 outline=RING, width=8)
    draw.ellipse([margin + 10, margin + 10, SIZE - margin - 10, SIZE - margin - 10],
                 outline=ACCENT, width=2)

    # Bottom sine waves (echo the animated UI background).
    draw_wave(draw, ACCENT, SIZE * 0.80, 9, 150, 0.0, 4)
    draw_wave(draw, REDBROWN, SIZE * 0.86, 6, 110, 1.3, 3)

    # Central "Я" glyph with a soft shadow.
    font = load_font(170)
    glyph = "Я"  # Cyrillic capital YA
    bbox = draw.textbbox((0, 0), glyph, font=font)
    gw, gh = bbox[2] - bbox[0], bbox[3] - bbox[1]
    gx = (SIZE - gw) / 2 - bbox[0]
    gy = (SIZE - gh) / 2 - bbox[1] - 12

    shadow = Image.new("RGBA", (SIZE, SIZE), (0, 0, 0, 0))
    sdraw = ImageDraw.Draw(shadow)
    sdraw.text((gx + 6, gy + 8), glyph, font=font, fill=(0, 0, 0, 150))
    shadow = shadow.filter(ImageFilter.GaussianBlur(5))
    img.paste(shadow, (0, 0), shadow)

    draw.text((gx, gy), glyph, font=font, fill=CREAM)

    img.save(out_path, "JPEG", quality=92)
    print(f"wrote {out_path} ({out_path.stat().st_size} bytes, {SIZE}x{SIZE})")


def main() -> int:
    out = Path(sys.argv[1]) if len(sys.argv) > 1 else Path("icon.jpg")
    generate(out)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())

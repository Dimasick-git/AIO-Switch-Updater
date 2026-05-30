#!/usr/bin/env python3
"""Procedurally generate the Ryazhenka Updater app icon.

Produces a 256x256 JPEG matching the latest brand: a glowing orange Я set
inside a rounded square frame on a near-black background. No text — just
the mark.

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

# Near-black background with a faint warm gradient.
BG_DEEP  = (8, 6, 4)
BG_WARM  = (28, 18, 12)
# Glow + glyph colour ramp (light yellow → orange → red).
GLOW_OUT = (255, 64, 0)
GLOW_MID = (255, 140, 40)
GLOW_IN  = (255, 220, 140)


def radial_background(img: Image.Image) -> None:
    cx = cy = SIZE / 2
    max_r = math.hypot(cx, cy)
    px = img.load()
    for y in range(SIZE):
        for x in range(SIZE):
            t = min(1.0, math.hypot(x - cx, y - cy) / max_r)
            # bright at centre, dark at the corners — emphasises the glyph.
            mix = t ** 1.2
            px[x, y] = tuple(
                int(BG_WARM[i] + (BG_DEEP[i] - BG_WARM[i]) * mix)
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


def glow_layer(make_path) -> Image.Image:
    """Composite a multi-pass blur to create a soft outer glow."""
    canvas = Image.new("RGBA", (SIZE, SIZE), (0, 0, 0, 0))
    for radius, color, alpha in (
        (28, GLOW_OUT, 90),
        (16, GLOW_OUT, 140),
        (8,  GLOW_MID, 200),
    ):
        layer = Image.new("RGBA", (SIZE, SIZE), (0, 0, 0, 0))
        draw = ImageDraw.Draw(layer)
        make_path(draw, color + (alpha,))
        layer = layer.filter(ImageFilter.GaussianBlur(radius))
        canvas = Image.alpha_composite(canvas, layer)
    return canvas


def draw_frame(draw: ImageDraw.ImageDraw, color) -> None:
    # Rounded-square frame, stroked.
    inset = 38
    draw.rounded_rectangle(
        [inset, inset, SIZE - inset, SIZE - inset],
        radius=44, outline=color, width=6,
    )


def draw_glyph(draw: ImageDraw.ImageDraw, color) -> None:
    """Render Latin R mirrored horizontally so it looks like the brand mark.

    Drawing R + mirroring uses the Latin font's hinting / weight curves,
    which matches the reference logo better than Cyrillic Я (whose glyph
    differs from font to font and tends to be lighter)."""
    font = load_font(160)
    glyph = "R"
    # Measure on a throwaway canvas (no `draw.textbbox` on an arbitrary ctx).
    measure = Image.new("L", (1, 1))
    md = ImageDraw.Draw(measure)
    bbox = md.textbbox((0, 0), glyph, font=font)
    gw, gh = bbox[2] - bbox[0], bbox[3] - bbox[1]

    glyph_layer = Image.new("RGBA", (gw + 8, gh + 8), (0, 0, 0, 0))
    gd = ImageDraw.Draw(glyph_layer)
    gd.text((-bbox[0] + 4, -bbox[1] + 4), glyph, font=font, fill=color)
    glyph_layer = glyph_layer.transpose(Image.FLIP_LEFT_RIGHT)

    gx = int((SIZE - glyph_layer.width) / 2)
    gy = int((SIZE - glyph_layer.height) / 2 - 4)

    # The caller passes a `draw` bound to the target image. We need that
    # image — pull it out of the draw object's private _image attribute,
    # which Pillow exposes for exactly this kind of compositing.
    target = draw._image
    target.alpha_composite(glyph_layer, dest=(gx, gy))


def generate(out_path: Path) -> None:
    base = Image.new("RGB", (SIZE, SIZE), BG_DEEP)
    radial_background(base)
    base = base.convert("RGBA")

    # Soft glow underneath the frame + glyph.
    base = Image.alpha_composite(base, glow_layer(draw_frame))
    base = Image.alpha_composite(base, glow_layer(draw_glyph))

    # Sharp top layer.
    sharp = Image.new("RGBA", (SIZE, SIZE), (0, 0, 0, 0))
    sdraw = ImageDraw.Draw(sharp)
    draw_frame(sdraw, GLOW_MID + (255,))
    draw_glyph(sdraw, GLOW_IN + (255,))
    base = Image.alpha_composite(base, sharp)

    base.convert("RGB").save(out_path, "JPEG", quality=92)
    print(f"wrote {out_path} ({out_path.stat().st_size} bytes, {SIZE}x{SIZE})")


def main() -> int:
    out = Path(sys.argv[1]) if len(sys.argv) > 1 else Path("icon.jpg")
    generate(out)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())

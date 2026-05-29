#pragma once

/**
 * @file ryazhenka_nvg.hpp
 * @brief Shared NanoVG drawing helpers for the Ryazhenka widget toolkit.
 * @author Dimasick-git
 *
 * Header-only, inline. Centralises the rounded-card / glow / gradient-text /
 * icon boilerplate so each custom view stays small. All helpers build their
 * paints inside the call (no cached NVGpaint) so live palette switching never
 * shows stale colours.
 */

#include <borealis.hpp>

namespace ryazhenka::nvg {

/// Filled rounded rect with an optional border.
inline void roundedCard(NVGcontext* vg, float x, float y, float w, float h, float r,
                        NVGcolor fill, NVGcolor border, float borderWidth = 1.5f) {
    nvgBeginPath(vg);
    nvgRoundedRect(vg, x, y, w, h, r);
    nvgFillColor(vg, fill);
    nvgFill(vg);
    if (border.a > 0.0f) {
        nvgBeginPath(vg);
        nvgRoundedRect(vg, x + borderWidth * 0.5f, y + borderWidth * 0.5f,
                       w - borderWidth, h - borderWidth, r);
        nvgStrokeColor(vg, border);
        nvgStrokeWidth(vg, borderWidth);
        nvgStroke(vg);
    }
}

/// Soft outer glow around a rounded rect; `alpha` (0..1) scales intensity.
inline void glow(NVGcontext* vg, float x, float y, float w, float h, float r,
                 NVGcolor color, float spread, float alpha) {
    if (alpha <= 0.001f)
        return;
    NVGcolor inner = color;
    inner.a = color.a * alpha;
    NVGcolor outer = nvgRGBA(0, 0, 0, 0);
    NVGpaint paint = nvgBoxGradient(vg, x, y, w, h, r + spread * 0.5f, spread, inner, outer);
    nvgBeginPath(vg);
    nvgRect(vg, x - spread * 2.0f, y - spread * 2.0f, w + spread * 4.0f, h + spread * 4.0f);
    nvgRoundedRect(vg, x, y, w, h, r);
    nvgPathWinding(vg, NVG_HOLE);
    nvgFillPaint(vg, paint);
    nvgFill(vg);
}

/// Left-aligned, vertically-centred text in a horizontal colour gradient.
inline void gradientText(NVGcontext* vg, int font, float size, float x, float y,
                         const char* text, NVGcolor a, NVGcolor b, float width) {
    NVGpaint paint = nvgLinearGradient(vg, x, y, x + width, y, a, b);
    nvgFontFaceId(vg, font);
    nvgFontSize(vg, size);
    nvgTextAlign(vg, NVG_ALIGN_LEFT | NVG_ALIGN_MIDDLE);
    nvgFillPaint(vg, paint);
    nvgText(vg, x, y, text, nullptr);
}

/// Centred material-icon glyph (from FontStash::material).
inline void iconGlyph(NVGcontext* vg, int materialFont, float size, float cx, float cy,
                      const char* utf8glyph, NVGcolor colour) {
    nvgFontFaceId(vg, materialFont);
    nvgFontSize(vg, size);
    nvgTextAlign(vg, NVG_ALIGN_CENTER | NVG_ALIGN_MIDDLE);
    nvgFillColor(vg, colour);
    nvgText(vg, cx, cy, utf8glyph, nullptr);
}

}  // namespace ryazhenka::nvg

#pragma once

/**
 * @file ryazhenka_palette.hpp
 * @brief Colour palettes for the Ryazhenka visual language.
 * @author Dimasick-git
 *
 * A palette is a compact set of *semantic* colours. ryazhenka_theme.cpp expands
 * one palette into the ~40 fields of brls::Theme deterministically, so the whole
 * UI stays consistent and there is a single small place to tweak per palette.
 */

#include <cstdint>
#include <string_view>

namespace ryazhenka::theme {

enum class PaletteId {
    Ryazhenka,  // cream + red-brown dairy theme (default)
    Cyberpunk,  // fuchsia + cyan
    Gold,       // black + amber
    Ocean,      // deep blue + teal
    Standard,   // the original Horizon-style neutral palette (white + cyan)
    Slate,      // muted grey, no warm tint — neutral / professional
};

enum class Variant {
    Light,
    Dark,
};

constexpr int kPaletteCount = 6;

/// 8-bit RGBA colour, constexpr-friendly.
struct Rgba {
    std::uint8_t r = 0, g = 0, b = 0, a = 255;
};

/// Compact semantic palette. Expanded into brls::Theme by writePaletteInto().
struct Palette {
    Rgba bg;          ///< window background / GL clear colour
    Rgba surface;     ///< sidebar, panels, regular buttons
    Rgba text;        ///< primary text
    Rgba textDim;     ///< descriptions / faint values
    Rgba accent;      ///< active tab, primary button, list values
    Rgba accentText;  ///< text drawn on top of `accent`
    Rgba highlight1;  ///< focus highlight gradient (inner)
    Rgba highlight2;  ///< focus highlight gradient (outer)
    Rgba separator;   ///< separators / borders

    // Accents used only by our custom widgets (not part of brls::Theme).
    Rgba cardBg;      ///< card fill — keep alpha < 255 so the wave bg shows through
    Rgba cardBorder;  ///< card border
    Rgba glow;        ///< focus glow / accent particles
    Rgba waveA;       ///< animated background wave layer A
    Rgba waveB;       ///< animated background wave layer B
};

/// Returns the static palette for the given id + variant.
const Palette& getPalette(PaletteId id, Variant variant);

/// Parses a config.json palette string (case-insensitive); defaults to Ryazhenka.
PaletteId paletteFromString(std::string_view name);

/// Canonical lowercase id used in config.json.
const char* paletteToString(PaletteId id);

/// i18n key suffix for the palette's display name (menus/ryazhenka/palette/<x>).
const char* paletteI18nKey(PaletteId id);

}  // namespace ryazhenka::theme

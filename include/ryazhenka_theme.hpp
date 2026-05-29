#pragma once

/**
 * @file ryazhenka_theme.hpp
 * @brief Custom borealis theme built from Ryazhenka palettes, with live swap.
 * @author Dimasick-git
 *
 * borealis reads ctx->theme->field every frame from a stable Theme* held by the
 * Application's theme-variants wrapper. So we can switch palette at runtime just
 * by overwriting the fields of the live Theme objects in place — the whole UI
 * repaints with the new colours on the next frame. No restart required for a
 * palette change (a Light<->Dark *variant* flip is the only thing that needs a
 * relaunch, because borealis picks the variant once at init).
 */

#include <borealis.hpp>

#include "ryazhenka_palette.hpp"

namespace ryazhenka::theme {

/// Expands a semantic Palette into every field of a brls::Theme (in place).
void writePaletteInto(brls::Theme* theme, const Palette& palette);

/// Builds the Light/Dark wrapper to hand to brls::Application::init().
/// Caller transfers ownership to the Application.
brls::LibraryViewsThemeVariantsWrapper* makeWrapper();

/// Reads the persisted palette from config.json (defaults to Ryazhenka) and
/// applies it. Call once after Application::init(), before pushing any view.
void loadAndApplyFromConfig();

/// Switches palette live (mutates the live Theme objects). Must be called from
/// the UI thread only. When `persist` is true the choice is written to
/// config.json so it survives relaunch.
void applyPalette(PaletteId id, bool persist = true);

/// Currently active palette id.
PaletteId current();

/// The active palette resolved for the current Light/Dark variant — use this in
/// custom widgets / the background for our extra accent colours.
const Palette& currentPalette();

/// Convenience for cycling palettes (used by the temporary Phase-1 switcher).
PaletteId next(PaletteId id);

}  // namespace ryazhenka::theme

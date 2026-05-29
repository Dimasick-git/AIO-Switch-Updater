#pragma once

/**
 * @file ryazhenka_settings_screen.hpp
 * @brief Ryazhenka settings tab — live palette switch, haptics, background.
 * @author Dimasick-git
 *
 * Built entirely from the custom widget toolkit (RyazhenkaCard in a transparent
 * brls::List). Selecting a palette calls theme::applyPalette() so the whole UI —
 * including this screen — recolours on the next frame. Toggles persist to
 * config.json.
 */

#include <borealis.hpp>
#include <vector>

#include "ryazhenka_card.hpp"
#include "ryazhenka_palette.hpp"

namespace ryazhenka {

class SettingsScreen : public brls::List {
public:
    SettingsScreen();

private:
    void refreshPaletteChecks();
    void refreshHaptics();
    void refreshBackground();

    struct PaletteRow {
        theme::PaletteId id;
        RyazhenkaCard* card;
    };
    std::vector<PaletteRow> paletteRows;
    RyazhenkaCard* hapticsToggle = nullptr;
    RyazhenkaCard* hapticsStrength = nullptr;
    RyazhenkaCard* backgroundToggle = nullptr;
};

}  // namespace ryazhenka

#pragma once

/**
 * @file ryazhenka_settings_screen.hpp
 * @brief Ryazhenka settings tab — live palette switch, haptics, background.
 * @author Dimasick-git
 *
 * Built from brls::ListItem. Selecting a palette calls theme::applyPalette()
 * so the whole UI — including this screen — recolours on the next frame.
 * Toggles persist to config.json.
 */

#include <borealis.hpp>
#include <vector>

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
        brls::ListItem* card;
    };
    std::vector<PaletteRow> paletteRows;
    brls::ListItem* hapticsToggle = nullptr;
    brls::ListItem* hapticsStrength = nullptr;
    brls::ListItem* backgroundToggle = nullptr;
};

}  // namespace ryazhenka

#pragma once

/**
 * @file ryazhenka_status_tab.hpp
 * @brief Live system dashboard tab — 4 NanoVG charts in a 2×2 grid.
 * @author Dimasick-git
 */

#include <borealis.hpp>

#include "ryazhenka_card.hpp"

namespace ryazhenka {

class StatusTab : public brls::BoxLayout {
public:
    StatusTab();

    void willAppear(bool resetState) override;
    void willDisappear(bool resetState) override;
    void frame(brls::FrameContext* ctx) override;

private:
    // Hidden as soon as the Sampler thread returns a first valid snapshot, or
    // after a short grace period so retail/applet units (where psm/ts may never
    // report "ok") don't sit on the placeholder forever.
    brls::Label* loadingLabel = nullptr;
    int loading_frames_ = 0;

    // CFW health summary, refreshed each time the tab becomes visible.
    brls::ListItem* healthCard = nullptr;
    void refreshHealth();

    // Header label (sysinfo one-liner). sysinfo::collect() iterates
    // /atmosphere/contents and /atmosphere/exefs_patches which can be slow on
    // a packed SD card; we keep the pointer so willAppear() can update the
    // text the first time the tab actually opens.
    brls::Label* headerLabel = nullptr;
    void refreshHeader();
};

} // namespace ryazhenka

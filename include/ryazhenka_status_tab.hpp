#pragma once

/**
 * @file ryazhenka_status_tab.hpp
 * @brief Live system dashboard tab — 4 NanoVG charts in a 2×2 grid.
 * @author Dimasick-git
 */

#include <borealis.hpp>

namespace ryazhenka {

class StatusTab : public brls::BoxLayout {
public:
    StatusTab();

    void willAppear(bool resetState) override;
    void willDisappear(bool resetState) override;
    void frame(brls::FrameContext* ctx) override;

private:
    // Hidden as soon as the Sampler thread returns a first valid snapshot.
    brls::Label* loadingLabel = nullptr;
};

} // namespace ryazhenka

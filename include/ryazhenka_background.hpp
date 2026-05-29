#pragma once

/**
 * @file ryazhenka_background.hpp
 * @brief Animated procedural background — layered sine waves + drifting motes.
 * @author Dimasick-git
 *
 * Drawn behind the whole view stack. Colours come from the live palette, so it
 * recolours instantly when the user switches themes. The framebuffer is cleared
 * every frame, so we repaint every frame and keep the geometry cheap instead of
 * trying to throttle (which would flicker).
 */

#include <borealis.hpp>

namespace ryazhenka {

class WaveBackground : public brls::Background {
public:
    WaveBackground();

    void preFrame() override;   // advances the animation phase
    void postFrame() override;  // no-op

    void draw(NVGcontext* vg, int x, int y, unsigned width, unsigned height,
              brls::Style* style, brls::FrameContext* ctx) override;
    void layout(NVGcontext* vg, brls::Style* style, brls::FontStash* stash) override;

    /// Runtime toggle for the animated waves (settings screen). When disabled
    /// only the flat palette background remains. Persisted by the caller.
    static void setEnabled(bool enabled);
    static bool isEnabled();

private:
    float phase_ = 0.0f;
};

}  // namespace ryazhenka

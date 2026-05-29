#pragma once

/**
 * @file ryazhenka_splash.hpp
 * @brief Animated launch splash — branded fade-in over the wave background.
 * @author Dimasick-git
 *
 * Drawn after Application::init() and before MainFrame is constructed. After a
 * short on-screen dwell it builds the MainFrame (which performs the upstream
 * nx-links download) and replaces itself on the view stack.
 */

#include <borealis.hpp>
#include <functional>
#include <string>

namespace ryazhenka {

class Splash : public brls::View {
public:
    /// `next` is invoked once, on the UI thread, when the splash is ready to
    /// hand off to whatever should come next (typically pushing MainFrame and
    /// popping the splash). It is called from frame() after a brief dwell so
    /// the user sees the branded screen before the (blocking) MainFrame
    /// construction kicks in.
    explicit Splash(std::function<void()> next);

    ~Splash();

    void draw(NVGcontext* vg, int x, int y, unsigned width, unsigned height,
              brls::Style* style, brls::FrameContext* ctx) override;
    void layout(NVGcontext* vg, brls::Style* style, brls::FontStash* stash) override;
    void frame(brls::FrameContext* ctx) override;
    brls::View* getDefaultFocus() override { return nullptr; }  // not focusable

private:
    std::function<void()> next_;
    int frames_ = 0;
    bool handed_off_ = false;
    float in_alpha_ = 0.0f;
    float out_alpha_ = 1.0f;  // dims at the very end

    // Lazily-created NanoVG texture for the cached release banner. -1 means
    // "no texture yet" (first launch with empty cache, or load failed).
    int bannerTexture_ = -1;
    int bannerW_ = 0;
    int bannerH_ = 0;
};

}  // namespace ryazhenka

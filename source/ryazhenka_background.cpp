#include "ryazhenka_background.hpp"

#include <cmath>

#include "constants.hpp"
#include "fs.hpp"
#include "ryazhenka_haptics.hpp"
#include "ryazhenka_theme.hpp"

namespace ryazhenka {

namespace {

constexpr float kTwoPi = 6.28318530718f;

bool g_enabled = true;

NVGcolor col(const theme::Rgba& c, std::uint8_t a) {
    return nvgRGBA(c.r, c.g, c.b, a);
}

}  // namespace

void WaveBackground::setEnabled(bool enabled) {
    g_enabled = enabled;
}

bool WaveBackground::isEnabled() {
    return g_enabled;
}

WaveBackground::WaveBackground() {
    // Backgrounds don't take focus and never need layout invalidation for their
    // own sake; we repaint every frame regardless. Honour the persisted toggle.
    try {
        nlohmann::ordered_json cfg = fs::parseJsonFile(CONFIG_FILE);
        if (cfg.is_object() && cfg.contains("ryazhenka_background") &&
            cfg["ryazhenka_background"].is_boolean())
            g_enabled = cfg["ryazhenka_background"].get<bool>();
    } catch (...) {
        // keep default (enabled)
    }
}

void WaveBackground::preFrame() {
    // Advance the phase by real time so the waves drift at a constant speed
    // independent of frame rate. delta is in milliseconds.
    float delta = brls::menu_animation_get_delta_time();
    phase_ += delta * 0.0012f;
    if (phase_ > kTwoPi * 1024.0f)
        phase_ -= kTwoPi * 1024.0f;  // keep the float bounded over long sessions

    // The background is the one view guaranteed to run every frame, so we drive
    // the (non-blocking) haptics scheduler from here.
    haptics::tick();
}

void WaveBackground::postFrame() {}

void WaveBackground::layout(NVGcontext* /*vg*/, brls::Style* /*style*/, brls::FontStash* /*stash*/) {}

void WaveBackground::draw(NVGcontext* vg, int x, int y, unsigned width, unsigned height,
                          brls::Style* /*style*/, brls::FrameContext* /*ctx*/) {
    const auto& pal = theme::currentPalette();

    const float w = static_cast<float>(width);
    const float h = static_cast<float>(height);
    const float fx = static_cast<float>(x);
    const float fy = static_cast<float>(y);

    // Subtle vertical wash over the cleared background to add depth.
    NVGpaint wash = nvgLinearGradient(vg, fx, fy, fx, fy + h,
                                      col(pal.bg, 0), col(pal.waveA, 26));
    nvgBeginPath(vg);
    nvgRect(vg, fx, fy, w, h);
    nvgFillPaint(vg, wash);
    nvgFill(vg);

    if (!g_enabled)
        return;  // flat palette wash only

    struct Layer {
        float amp;     // peak height in px
        float len;     // wavelength in px
        float speed;   // phase multiplier
        float yFrac;   // baseline as a fraction of height
        std::uint8_t alpha;
        const theme::Rgba& colour;
    };
    const Layer layers[3] = {
        {26.0f, 520.0f, 0.6f, 0.78f, 56, pal.waveA},
        {18.0f, 360.0f, 0.9f, 0.85f, 46, pal.waveB},
        {12.0f, 240.0f, 1.4f, 0.92f, 36, pal.glow},
    };

    for (const auto& L : layers) {
        const float baseY = fy + h * L.yFrac;
        nvgBeginPath(vg);
        nvgMoveTo(vg, fx, fy + h);
        for (float px = 0.0f; px <= w; px += 24.0f) {
            const float yy = baseY + std::sin((px / L.len) * kTwoPi + phase_ * L.speed) * L.amp;
            nvgLineTo(vg, fx + px, yy);
        }
        nvgLineTo(vg, fx + w, fy + h);
        nvgClosePath(vg);

        NVGpaint fill = nvgLinearGradient(vg, fx, baseY - L.amp, fx, fy + h,
                                          col(L.colour, L.alpha), col(L.colour, 0));
        nvgFillPaint(vg, fill);
        nvgFill(vg);
    }

    // Drifting motes for a touch of life.
    for (int i = 0; i < 18; ++i) {
        const float mx = std::fmod(static_cast<float>(i) * 97.0f + phase_ * 9.0f, w);
        const float my = std::fmod(h * 0.18f + std::sin(phase_ * 0.5f + static_cast<float>(i)) * 36.0f +
                                       static_cast<float>(i) * 11.0f,
                                   h);
        nvgBeginPath(vg);
        nvgCircle(vg, fx + mx, fy + my, 1.6f);
        nvgFillColor(vg, col(pal.glow, 60));
        nvgFill(vg);
    }
}

}  // namespace ryazhenka

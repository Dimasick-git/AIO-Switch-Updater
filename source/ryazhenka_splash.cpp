#include "ryazhenka_splash.hpp"

#include <cmath>

#include "ryazhenka_banner.hpp"
#include "ryazhenka_config.hpp"
#include "ryazhenka_nvg.hpp"
#include "ryazhenka_theme.hpp"

namespace ryazhenka {

namespace {

constexpr int kFadeInFrames = 24;     // ~0.4 s @ 60 fps
constexpr int kDwellFrames  = 78;     // ~1.3 s — long enough to register, short enough to feel snappy
constexpr int kHandOffFrame = kFadeInFrames + kDwellFrames;

NVGcolor col(const theme::Rgba& c, std::uint8_t a = 255) {
    return nvgRGBA(c.r, c.g, c.b, a);
}

}  // namespace

Splash::Splash(std::function<void()> next) : next_(std::move(next)) {
    // The splash should cover the whole window. The Application sets our
    // boundaries to the content area when we are pushed onto the stack.
}

Splash::~Splash() {
    if (this->bannerTexture_ != -1) {
        if (auto* vg = brls::Application::getNVGContext())
            nvgDeleteImage(vg, this->bannerTexture_);
    }
}

void Splash::layout(NVGcontext* /*vg*/, brls::Style* /*style*/, brls::FontStash* /*stash*/) {}

void Splash::frame(brls::FrameContext* ctx) {
    ++this->frames_;
    if (this->frames_ <= kFadeInFrames)
        this->in_alpha_ = std::min(1.0f, static_cast<float>(this->frames_) / kFadeInFrames);
    else
        this->in_alpha_ = 1.0f;

    if (this->frames_ >= kHandOffFrame && !this->handed_off_) {
        this->handed_off_ = true;
        // Trigger the caller — typically: construct MainFrame, push it,
        // pop the splash. We don't push/pop ourselves here so the splash has
        // no special knowledge of what comes next.
        if (this->next_)
            this->next_();
    }

    brls::View::frame(ctx);
}

void Splash::draw(NVGcontext* vg, int x, int y, unsigned width, unsigned height,
                  brls::Style* /*style*/, brls::FrameContext* ctx) {
    const auto& pal = theme::currentPalette();

    const float fx = static_cast<float>(x);
    const float fy = static_cast<float>(y);
    const float w  = static_cast<float>(width);
    const float h  = static_cast<float>(height);
    const float cx = fx + w * 0.5f;
    const float cy = fy + h * 0.5f;

    // Lazy-load the cached release banner texture on the first frame that has
    // a valid NanoVG context. If the cache is empty (first launch) the texture
    // stays -1 and we just render the text-only splash.
    if (this->bannerTexture_ == -1) {
        const std::string path = banner::cachedPath();
        if (!path.empty()) {
            this->bannerTexture_ = nvgCreateImage(vg, path.c_str(), 0);
            if (this->bannerTexture_ != -1)
                nvgImageSize(vg, this->bannerTexture_, &this->bannerW_, &this->bannerH_);
        }
    }

    // Banner backdrop — cover-fit (centred, scaled to fill, cropped to bounds).
    // Faded by in_alpha_ so it ramps in with the rest of the splash.
    if (this->bannerTexture_ != -1 && this->bannerW_ > 0 && this->bannerH_ > 0) {
        const float scale = std::max(w / this->bannerW_, h / this->bannerH_);
        const float iw = this->bannerW_ * scale;
        const float ih = this->bannerH_ * scale;
        const float ix = fx + (w - iw) * 0.5f;
        const float iy = fy + (h - ih) * 0.5f;
        NVGpaint paint = nvgImagePattern(vg, ix, iy, iw, ih, 0.0f,
                                         this->bannerTexture_, this->in_alpha_);
        nvgBeginPath(vg);
        nvgRect(vg, fx, fy, w, h);
        nvgFillPaint(vg, paint);
        nvgFill(vg);
    }

    // Faint scrim over background + banner so the splash text reads cleanly,
    // fading in with the rest of the content. Stronger when a banner is shown
    // so the text isn't lost against complex imagery.
    const std::uint8_t scrimAlpha = static_cast<std::uint8_t>(
        (this->bannerTexture_ != -1 ? 175.0f : 140.0f) * this->in_alpha_);
    NVGcolor scrim = col(pal.bg, scrimAlpha);
    nvgBeginPath(vg);
    nvgRect(vg, fx, fy, w, h);
    nvgFillColor(vg, scrim);
    nvgFill(vg);

    // Glowing brand glyph in the centre.
    NVGcolor glowC = col(pal.glow);
    glowC.a = static_cast<unsigned char>(180.0f * this->in_alpha_);
    nvg::glow(vg, cx - 64.0f, cy - 80.0f, 128.0f, 128.0f, 24.0f,
              glowC, 40.0f, this->in_alpha_);

    nvgFontFaceId(vg, ctx->fontStash->regular);
    nvgFontSize(vg, 150.0f);
    nvgTextAlign(vg, NVG_ALIGN_CENTER | NVG_ALIGN_MIDDLE);
    nvgFillColor(vg, col(pal.accent, static_cast<std::uint8_t>(255.0f * this->in_alpha_)));
    nvgText(vg, cx, cy - 20.0f, "\xD0\xAF", nullptr);  // Cyrillic Я

    // Localised title underneath.
    nvgFontSize(vg, 36.0f);
    nvgFillColor(vg, col(pal.text, static_cast<std::uint8_t>(255.0f * this->in_alpha_)));
    const std::string title(kAppTitleLocalized);
    nvgText(vg, cx, cy + 88.0f, title.c_str(), nullptr);

    // Subtle tagline.
    nvgFontSize(vg, 18.0f);
    nvgFillColor(vg, col(pal.textDim, static_cast<std::uint8_t>(220.0f * this->in_alpha_)));
    nvgText(vg, cx, cy + 124.0f, "loading…", nullptr);
}

}  // namespace ryazhenka

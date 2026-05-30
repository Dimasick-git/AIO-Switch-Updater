#include "ryazhenka_splash.hpp"

#include <cmath>

#include "ryazhenka_banner.hpp"
#include "ryazhenka_config.hpp"
#include "ryazhenka_nvg.hpp"
#include "ryazhenka_theme.hpp"

namespace ryazhenka {

namespace {

constexpr int kFadeInFrames = 24;     // ~0.4 s @ 60 fps
constexpr int kDwellFrames  = 96;     // ~1.6 s
constexpr int kHandOffFrame = kFadeInFrames + kDwellFrames;

NVGcolor col(const theme::Rgba& c, std::uint8_t a = 255) {
    return nvgRGBA(c.r, c.g, c.b, a);
}

// Milky-cream colours used by the splash (the "ряженка молочко" look) —
// independent of the active in-app palette so the launch screen is always
// the same warm dairy tone the user asked for.
constexpr NVGcolor kCream      = {{{0.957f, 0.918f, 0.835f, 1.0f}}};   // #F4EAD5
constexpr NVGcolor kCreamSoft  = {{{0.980f, 0.953f, 0.878f, 1.0f}}};   // #FAF3E0
constexpr NVGcolor kRimWarm    = {{{0.255f, 0.157f, 0.110f, 1.0f}}};   // #41281C
constexpr NVGcolor kAccentOrange = {{{1.000f, 0.494f, 0.157f, 1.0f}}}; // #FF7E28
constexpr NVGcolor kAccentDeep   = {{{0.917f, 0.290f, 0.110f, 1.0f}}}; // #EA4A1C

NVGcolor withAlpha(NVGcolor c, float a) {
    c.a = std::clamp(a, 0.0f, 1.0f);
    return c;
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
                  brls::Style* /*style*/, brls::FrameContext* /*ctx*/) {
    const float fx = static_cast<float>(x);
    const float fy = static_cast<float>(y);
    const float w  = static_cast<float>(width);
    const float h  = static_cast<float>(height);
    const float cx = fx + w * 0.5f;
    const float cy = fy + h * 0.5f;

    // Lazy-load the cached release banner texture on the first frame.
    // No cache → texture stays -1 and the splash is just the cream wash.
    if (this->bannerTexture_ == -1) {
        const std::string path = banner::cachedPath();
        if (!path.empty()) {
            this->bannerTexture_ = nvgCreateImage(vg, path.c_str(), 0);
            if (this->bannerTexture_ != -1)
                nvgImageSize(vg, this->bannerTexture_, &this->bannerW_, &this->bannerH_);
        }
    }

    // Cream wash filling the whole window — gives the splash its dairy-bright
    // base regardless of the current in-app palette.
    {
        NVGpaint wash = nvgLinearGradient(vg, fx, fy, fx, fy + h,
                                          withAlpha(kCreamSoft, this->in_alpha_),
                                          withAlpha(kCream, this->in_alpha_));
        nvgBeginPath(vg);
        nvgRect(vg, fx, fy, w, h);
        nvgFillPaint(vg, wash);
        nvgFill(vg);
    }

    // If we have a cached banner, blend it in faintly so the release artwork
    // still shows through the cream — never enough to obscure the centred Я.
    if (this->bannerTexture_ != -1 && this->bannerW_ > 0 && this->bannerH_ > 0) {
        const float scale = std::max(w / this->bannerW_, h / this->bannerH_);
        const float iw = this->bannerW_ * scale;
        const float ih = this->bannerH_ * scale;
        const float ix = fx + (w - iw) * 0.5f;
        const float iy = fy + (h - ih) * 0.5f;
        NVGpaint paint = nvgImagePattern(vg, ix, iy, iw, ih, 0.0f,
                                         this->bannerTexture_, 0.18f * this->in_alpha_);
        nvgBeginPath(vg);
        nvgRect(vg, fx, fy, w, h);
        nvgFillPaint(vg, paint);
        nvgFill(vg);
    }

    // Centred rounded-square frame around the Я, mirroring the new app icon.
    constexpr float kFrameSize = 188.0f;
    constexpr float kFrameRadius = 40.0f;
    const float fxL = cx - kFrameSize * 0.5f;
    const float fyT = cy - kFrameSize * 0.5f;

    // Outer glow under the frame — orange halo radiating out.
    nvg::glow(vg, fxL, fyT, kFrameSize, kFrameSize, kFrameRadius,
              withAlpha(kAccentOrange, 0.9f * this->in_alpha_), 60.0f, this->in_alpha_);

    // Frame stroke.
    nvgBeginPath(vg);
    nvgRoundedRect(vg, fxL, fyT, kFrameSize, kFrameSize, kFrameRadius);
    nvgStrokeColor(vg, withAlpha(kAccentDeep, this->in_alpha_));
    nvgStrokeWidth(vg, 5.0f);
    nvgStroke(vg);

    // Glowing brand mark — Latin "R" rendered with a horizontal mirror so it
    // reads as the Ryazhenka logo, exactly like the icon. Using R + flip
    // (rather than Cyrillic Я) keeps the glyph weight and curves matching the
    // reference artwork, since R is what the fonts hint and kern for.
    nvgSave(vg);
    nvgTranslate(vg, cx, cy);
    nvgScale(vg, -1.0f, 1.0f);
    nvgFontFaceId(vg, 0);  // FontStash::regular (id 0 is the default font slot).
    nvgFontSize(vg, 170.0f);
    nvgTextAlign(vg, NVG_ALIGN_CENTER | NVG_ALIGN_MIDDLE);
    // Two passes: a wider warm halo behind the glyph, then a sharp bright top.
    nvgFillColor(vg, withAlpha(kAccentDeep, 0.65f * this->in_alpha_));
    nvgText(vg, 0.5f, 0.5f, "R", nullptr);
    nvgFillColor(vg, withAlpha(kCream, this->in_alpha_));
    nvgText(vg, 0.0f, 0.0f, "R", nullptr);
    nvgRestore(vg);

    // A small breathing pulse below the frame to prove the app is alive even
    // when MainFrame is busy constructing (e.g. fetching nx-links). No text.
    const float pulse = 0.5f + 0.5f * std::sin(this->frames_ * 0.10f);
    const float dotR = 3.0f + 1.0f * pulse;
    const float dotY = cy + kFrameSize * 0.5f + 36.0f;
    constexpr int kDots = 3;
    for (int i = 0; i < kDots; ++i) {
        const float ph = (this->frames_ * 0.10f) - i * 0.55f;
        const float a  = 0.35f + 0.55f * std::max(0.0f, std::sin(ph));
        nvgBeginPath(vg);
        nvgCircle(vg, cx + (i - 1) * 22.0f, dotY, dotR);
        nvgFillColor(vg, withAlpha(kRimWarm, a * this->in_alpha_));
        nvgFill(vg);
    }
}

}  // namespace ryazhenka

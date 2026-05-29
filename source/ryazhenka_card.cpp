#include "ryazhenka_card.hpp"

#include <cmath>

#include "ryazhenka_haptics.hpp"
#include "ryazhenka_nvg.hpp"
#include "ryazhenka_theme.hpp"

namespace i18n = brls::i18n;
using namespace i18n::literals;

namespace ryazhenka {

namespace {
constexpr unsigned kHeight = 78;
constexpr unsigned kHeightWithSub = 94;
constexpr float kCorner = 12.0f;

NVGcolor col(const theme::Rgba& c) {
    return nvgRGBA(c.r, c.g, c.b, c.a);
}
}  // namespace

RyazhenkaCard::RyazhenkaCard(std::string title, std::string subtitle,
                             std::string iconGlyph, std::string value)
    : title(std::move(title))
    , subtitle(std::move(subtitle))
    , icon(std::move(iconGlyph))
    , value(std::move(value)) {
    this->setHeight(this->subtitle.empty() ? kHeight : kHeightWithSub);
    this->registerAction("brls/hints/ok"_i18n, brls::Key::A, [this] { return this->onClick(); });
}

void RyazhenkaCard::setValue(const std::string& v) {
    this->value = v;
}

void RyazhenkaCard::setSubtitle(const std::string& s) {
    this->subtitle = s;
}

bool RyazhenkaCard::onClick() {
    haptics::click();
    this->playClickAnimation();
    this->triggerClickBurst();
    return this->clickEvent.fire(this);
}

void RyazhenkaCard::triggerClickBurst() {
    brls::menu_animation_ctx_tag tag = (uintptr_t)&this->clickBurst;
    brls::menu_animation_kill_by_tag(&tag);
    this->clickBurst = 0.0f;

    brls::menu_animation_ctx_entry_t entry;
    entry.easing_enum  = brls::EASING_OUT_QUAD;
    entry.tag          = tag;
    entry.duration     = 320.0f;
    entry.target_value = 1.0f;
    entry.subject      = &this->clickBurst;
    entry.cb           = [](void*) {};
    entry.tick         = [](void*) {};
    entry.userdata     = nullptr;
    brls::menu_animation_push(&entry);
}

void RyazhenkaCard::animateGlow(float target) {
    brls::menu_animation_ctx_tag tag = (uintptr_t)&this->glow;
    brls::menu_animation_kill_by_tag(&tag);

    brls::menu_animation_ctx_entry_t entry;
    entry.easing_enum = brls::EASING_OUT_QUAD;
    entry.tag         = tag;
    entry.duration    = 150.0f;
    entry.target_value = target;
    entry.subject     = &this->glow;
    entry.cb          = [](void*) {};
    entry.tick        = [](void*) {};
    entry.userdata    = nullptr;
    brls::menu_animation_push(&entry);
}

void RyazhenkaCard::onFocusGained() {
    brls::View::onFocusGained();
    this->animateGlow(1.0f);
    haptics::focus();
}

void RyazhenkaCard::onFocusLost() {
    brls::View::onFocusLost();
    this->animateGlow(0.0f);
}

void RyazhenkaCard::draw(NVGcontext* vg, int x, int y, unsigned width, unsigned height,
                         brls::Style* /*style*/, brls::FrameContext* ctx) {
    const auto& pal = theme::currentPalette();

    const float fx = static_cast<float>(x);
    const float fy = static_cast<float>(y);
    const float w  = static_cast<float>(width);
    const float h  = static_cast<float>(height);

    // Focus glow underneath the card.
    nvg::glow(vg, fx, fy, w, h, kCorner, col(pal.glow), 18.0f, this->glow);

    // Card body (translucent so the animated background shows through).
    NVGcolor border = nvgLerpRGBA(col(pal.cardBorder), col(pal.glow), this->glow);
    nvg::roundedCard(vg, fx, fy, w, h, kCorner, col(pal.cardBg), border, 1.5f);

    // Optional leading icon.
    float textX = fx + 18.0f;
    if (!this->icon.empty()) {
        nvg::iconGlyph(vg, ctx->fontStash->material, 30.0f, fx + 34.0f, fy + h * 0.5f,
                       this->icon.c_str(), ctx->theme->listItemValueColor);
        textX = fx + 64.0f;
    }

    // Title (+ subtitle).
    nvgFontFaceId(vg, ctx->fontStash->regular);
    nvgFillColor(vg, ctx->theme->textColor);
    nvgFontSize(vg, 23.0f);
    nvgTextAlign(vg, NVG_ALIGN_LEFT | NVG_ALIGN_MIDDLE);
    nvgText(vg, textX, this->subtitle.empty() ? fy + h * 0.5f : fy + h * 0.36f,
            this->title.c_str(), nullptr);

    if (!this->subtitle.empty()) {
        nvgFillColor(vg, ctx->theme->descriptionColor);
        nvgFontSize(vg, 16.0f);
        nvgText(vg, textX, fy + h * 0.68f, this->subtitle.c_str(), nullptr);
    }

    // Trailing value.
    if (!this->value.empty()) {
        nvgFillColor(vg, ctx->theme->listItemValueColor);
        nvgFontSize(vg, 20.0f);
        nvgTextAlign(vg, NVG_ALIGN_RIGHT | NVG_ALIGN_MIDDLE);
        nvgText(vg, fx + w - 18.0f, fy + h * 0.5f, this->value.c_str(), nullptr);
    }

    // Radial spark burst on click — 10 motes expanding from the card centre,
    // fading as they travel outward. Cheap (10 nvgCircle calls), visible only
    // for ~320 ms, otherwise the whole `if` is skipped.
    if (this->clickBurst > 0.0f && this->clickBurst < 1.0f) {
        const float bcx = fx + w * 0.5f;
        const float bcy = fy + h * 0.5f;
        const float radius = 24.0f + this->clickBurst * 68.0f;
        const float fade = 1.0f - this->clickBurst;
        const float dotR = 2.6f * fade + 0.4f;

        NVGcolor c = nvgRGBA(pal.glow.r, pal.glow.g, pal.glow.b,
                             static_cast<std::uint8_t>(220.0f * fade));
        nvgFillColor(vg, c);
        constexpr int kMotes = 10;
        for (int i = 0; i < kMotes; ++i) {
            const float a = (static_cast<float>(i) / kMotes) * 6.2831853f;
            nvgBeginPath(vg);
            nvgCircle(vg, bcx + std::cos(a) * radius, bcy + std::sin(a) * radius, dotR);
            nvgFill(vg);
        }
    }
}

}  // namespace ryazhenka

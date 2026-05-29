#include "ryazhenka_theme.hpp"

#include <algorithm>

#include "constants.hpp"
#include "fs.hpp"
#include "ryazhenka_config.hpp"
#include "ryazhenka_logger.hpp"

namespace ryazhenka::theme {

namespace {

constexpr const char kConfigKey[] = "ryazhenka_palette";

PaletteId g_current = PaletteId::Ryazhenka;

NVGcolor col(const Rgba& c) {
    return nvgRGBA(c.r, c.g, c.b, c.a);
}

NVGcolor colA(const Rgba& c, std::uint8_t a) {
    return nvgRGBA(c.r, c.g, c.b, a);
}

// Mixes two colours; t=0 -> a, t=1 -> b.
NVGcolor mix(const Rgba& a, const Rgba& b, float t) {
    auto lerp = [t](std::uint8_t x, std::uint8_t y) {
        return static_cast<std::uint8_t>(x + (y - x) * t);
    };
    return nvgRGBA(lerp(a.r, b.r), lerp(a.g, b.g), lerp(a.b, b.b), 255);
}

Variant currentVariant() {
    return brls::Application::getThemeVariant() == brls::ThemeVariant::DARK
               ? Variant::Dark
               : Variant::Light;
}

void saveToConfig(PaletteId id) {
    try {
        nlohmann::ordered_json cfg = fs::parseJsonFile(CONFIG_FILE);
        if (!cfg.is_object())
            cfg = nlohmann::ordered_json::object();
        cfg[kConfigKey] = paletteToString(id);
        fs::writeJsonToFile(cfg, CONFIG_FILE);
    } catch (...) {
        log::warn("theme: failed to persist palette to config.json");
    }
}

}  // namespace

void writePaletteInto(brls::Theme* t, const Palette& p) {
    // GL clear colour mirrors the window background (read every frame, uncached).
    t->backgroundColor[0] = p.bg.r / 255.0f;
    t->backgroundColor[1] = p.bg.g / 255.0f;
    t->backgroundColor[2] = p.bg.b / 255.0f;
    t->backgroundColorRGB = col(p.bg);

    t->textColor        = col(p.text);
    t->descriptionColor = col(p.textDim);

    t->notificationTextColor = col(p.text);
    t->backdropColor         = nvgRGBA(0, 0, 0, 178);

    t->separatorColor = col(p.separator);

    t->sidebarColor          = col(p.surface);
    t->activeTabColor        = col(p.accent);
    t->sidebarSeparatorColor = col(p.separator);

    t->highlightBackgroundColor = mix(p.bg, p.surface, 0.5f);
    t->highlightColor1          = col(p.highlight1);
    t->highlightColor2          = col(p.highlight2);

    t->listItemSeparatorColor  = col(p.separator);
    t->listItemValueColor      = col(p.accent);
    t->listItemFaintValueColor = col(p.textDim);

    t->tableEvenBackgroundColor = col(p.surface);
    t->tableBodyTextColor       = col(p.textDim);

    t->dropdownBackgroundColor = colA(p.bg, 230);

    t->nextStageBulletColor = col(p.textDim);

    t->spinnerBarColor = colA(p.accent, 160);

    t->headerRectangleColor = col(p.accent);

    t->buttonPrimaryEnabledBackgroundColor  = col(p.accent);
    t->buttonPrimaryDisabledBackgroundColor = col(p.surface);
    t->buttonPrimaryEnabledTextColor        = col(p.accentText);
    t->buttonPrimaryDisabledTextColor       = col(p.textDim);
    t->buttonBorderedBorderColor            = col(p.accent);
    t->buttonBorderedTextColor              = col(p.text);
    t->buttonRegularBackgroundColor         = col(p.surface);
    t->buttonRegularTextColor               = col(p.text);
    t->buttonRegularBorderColor             = col(p.separator);

    t->dialogColor                = col(p.surface);
    t->dialogBackdrop             = nvgRGBA(0, 0, 0, 100);
    t->dialogButtonColor          = col(p.accent);
    t->dialogButtonSeparatorColor = col(p.separator);

    t->scrollBarColor       = col(p.textDim);
    t->scrollBarAlphaNormal = 0.2f;
    t->scrollBarAlphaFull   = 0.5f;

    t->clickAnimationAlpha = 0.3f;
}

namespace {

struct RyazhenkaLight : brls::Theme {
    RyazhenkaLight() { writePaletteInto(this, getPalette(PaletteId::Ryazhenka, Variant::Light)); }
};

struct RyazhenkaDark : brls::Theme {
    RyazhenkaDark() { writePaletteInto(this, getPalette(PaletteId::Ryazhenka, Variant::Dark)); }
};

}  // namespace

brls::LibraryViewsThemeVariantsWrapper* makeWrapper() {
    return new brls::LibraryViewsThemeVariantsWrapper(new RyazhenkaLight(), new RyazhenkaDark());
}

void applyPalette(PaletteId id, bool persist) {
    auto* wrap = brls::Application::getThemeVariantsWrapper();
    if (!wrap) {
        log::warn("theme: no theme wrapper installed — cannot apply palette");
        return;
    }
    // Mutate BOTH variants so the result is correct regardless of which one the
    // system selected at init; the live one repaints next frame.
    writePaletteInto(wrap->getLightTheme(), getPalette(id, Variant::Light));
    writePaletteInto(wrap->getDarkTheme(), getPalette(id, Variant::Dark));
    g_current = id;
    if (persist)
        saveToConfig(id);
    log::info(std::string("theme: applied palette ") + paletteToString(id));
}

void loadAndApplyFromConfig() {
    PaletteId id = PaletteId::Ryazhenka;
    try {
        nlohmann::ordered_json cfg = fs::parseJsonFile(CONFIG_FILE);
        if (cfg.is_object() && cfg.contains(kConfigKey) && cfg[kConfigKey].is_string())
            id = paletteFromString(cfg[kConfigKey].get<std::string>());
    } catch (...) {
        // keep default
    }
    applyPalette(id, /*persist=*/false);
}

PaletteId current() {
    return g_current;
}

const Palette& currentPalette() {
    return getPalette(g_current, currentVariant());
}

PaletteId next(PaletteId id) {
    return static_cast<PaletteId>((static_cast<int>(id) + 1) % kPaletteCount);
}

}  // namespace ryazhenka::theme

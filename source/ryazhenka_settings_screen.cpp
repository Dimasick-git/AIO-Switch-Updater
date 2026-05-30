#include "ryazhenka_settings_screen.hpp"

#include <algorithm>
#include <filesystem>

#include "constants.hpp"
#include "fs.hpp"
#include "ryazhenka_audio.hpp"
#include "ryazhenka_background.hpp"
#include "ryazhenka_banner.hpp"
#include "ryazhenka_catalog.hpp"
#include "ryazhenka_config.hpp"
#include "ryazhenka_haptics.hpp"
#include "ryazhenka_theme.hpp"

namespace i18n = brls::i18n;
using namespace i18n::literals;

namespace ryazhenka {

namespace {

const char* kCheck = "✓";  // ✓

void setConfigKey(const char* key, const nlohmann::ordered_json& value) {
    try {
        nlohmann::ordered_json cfg = fs::parseJsonFile(CONFIG_FILE);
        if (!cfg.is_object())
            cfg = nlohmann::ordered_json::object();
        cfg[key] = value;
        fs::writeJsonToFile(cfg, CONFIG_FILE);
    } catch (...) {
        // best effort
    }
}

std::string onOff(bool on) {
    return on ? "menus/ryazhenka/settings/on"_i18n : "menus/ryazhenka/settings/off"_i18n;
}

}  // namespace

SettingsScreen::SettingsScreen() {
    // --- Palette ---
    this->addView(new brls::Header("menus/ryazhenka/settings/palette_header"_i18n));

    const theme::PaletteId ids[] = {
        theme::PaletteId::Ryazhenka,
        theme::PaletteId::Cyberpunk,
        theme::PaletteId::Gold,
        theme::PaletteId::Ocean,
    };
    for (theme::PaletteId id : ids) {
        auto* card = new RyazhenkaCard(brls::i18n::getStr(theme::paletteI18nKey(id)),
                                       "", "" /* palette icon */, "");
        card->getClickEvent()->subscribe([this, id](brls::View*) {
            theme::applyPalette(id, /*persist=*/true);
            haptics::success();
            this->refreshPaletteChecks();
        });
        this->addView(card);
        this->paletteRows.push_back({id, card});
    }

    // --- Haptics ---
    this->addView(new brls::Header("menus/ryazhenka/settings/haptics_header"_i18n));

    this->hapticsToggle = new RyazhenkaCard("menus/ryazhenka/settings/haptics_toggle"_i18n,
                                            "", "" /* vibration icon */, "");
    this->hapticsToggle->getClickEvent()->subscribe([this](brls::View*) {
        bool next = !haptics::isEnabled();
        haptics::setEnabled(next);
        setConfigKey("haptics_enabled", next);
        if (next)
            haptics::click();  // brief confirmation buzz
        this->refreshHaptics();
    });
    this->addView(this->hapticsToggle);

    this->hapticsStrength = new RyazhenkaCard("menus/ryazhenka/settings/haptics_strength"_i18n,
                                              "", "", "");
    this->hapticsStrength->getClickEvent()->subscribe([this](brls::View*) {
        // Cycle 33% -> 66% -> 100%.
        float cur = 1.0f;
        try {
            nlohmann::ordered_json cfg = fs::parseJsonFile(CONFIG_FILE);
            if (cfg.is_object() && cfg.contains("haptics_strength") && cfg["haptics_strength"].is_number())
                cur = cfg["haptics_strength"].get<float>();
        } catch (...) {}
        float next = cur >= 0.99f ? 0.33f : (cur >= 0.66f ? 1.0f : 0.66f);
        haptics::setStrength(next);
        setConfigKey("haptics_strength", next);
        haptics::pulse(next, 60);  // preview at the new strength
        this->refreshHaptics();
    });
    this->addView(this->hapticsStrength);

    // --- Background ---
    this->addView(new brls::Header("menus/ryazhenka/settings/background_header"_i18n));

    this->backgroundToggle = new RyazhenkaCard("menus/ryazhenka/settings/background_toggle"_i18n,
                                               "", "" /* image icon */, "");
    this->backgroundToggle->getClickEvent()->subscribe([this](brls::View*) {
        bool next = !WaveBackground::isEnabled();
        WaveBackground::setEnabled(next);
        setConfigKey("ryazhenka_background", next);
        haptics::click();
        this->refreshBackground();
    });
    this->addView(this->backgroundToggle);

    auto* audioToggle = new RyazhenkaCard("menus/ryazhenka/settings/audio_toggle"_i18n,
                                          "menus/ryazhenka/settings/audio_hint"_i18n, "", "");
    audioToggle->getClickEvent()->subscribe([audioToggle](brls::View*) {
        bool next = !audio::isEnabled();
        audio::setEnabled(next);
        setConfigKey("ryazhenka_audio_enabled", next);
        if (next)
            audio::click();
        audioToggle->setValue(onOff(next));
    });
    audioToggle->setValue(onOff(audio::isEnabled()));
    this->addView(audioToggle);

    // --- Caches ---
    this->addView(new brls::Header("menus/ryazhenka/settings/cache_header"_i18n));

    auto* clearBanner = new RyazhenkaCard("menus/ryazhenka/settings/clear_banner"_i18n);
    clearBanner->getClickEvent()->subscribe([](brls::View*) {
        std::error_code ec;
        std::filesystem::remove(kBannerCachePath, ec);
        haptics::success();
    });
    this->addView(clearBanner);

    auto* refreshBanner = new RyazhenkaCard("menus/ryazhenka/settings/refresh_banner"_i18n);
    refreshBanner->getClickEvent()->subscribe([](brls::View*) {
        // Safe to spawn the curl threads here: the user is already navigating
        // the UI on the Settings tab, well past the first frame. The startup
        // rule that ca62519 enforced only forbids this from main() / first-tab
        // ctors. Refresh both the banner artwork and the on-SD nx-links cache
        // so the next launch comes up instantly with current data.
        ryazhenka::banner::refreshAsync();
        ryazhenka::catalog::refreshAsync();
        haptics::success();
    });
    this->addView(refreshBanner);

    auto* clearSig = new RyazhenkaCard("menus/ryazhenka/settings/clear_sigpatches"_i18n);
    clearSig->getClickEvent()->subscribe([](brls::View*) {
        std::error_code ec;
        std::filesystem::remove(kSigpatchesRemoteCachePath, ec);
        haptics::success();
    });
    this->addView(clearSig);

    auto* clearLog = new RyazhenkaCard("menus/ryazhenka/settings/clear_log"_i18n);
    clearLog->getClickEvent()->subscribe([](brls::View*) {
        std::error_code ec;
        std::filesystem::remove(kLogFilePath, ec);
        haptics::success();
    });
    this->addView(clearLog);

    // --- About ---
    this->addView(new brls::Header("menus/ryazhenka/settings/about_header"_i18n));
    auto* version = new RyazhenkaCard(std::string(kAppTitleLocalized), "", "", APP_VERSION);
    this->addView(version);
    auto* author = new RyazhenkaCard("menus/ryazhenka/settings/author"_i18n, "",
                                     "", std::string(kAppAuthor));
    this->addView(author);

    this->refreshPaletteChecks();
    this->refreshHaptics();
    this->refreshBackground();
}

void SettingsScreen::refreshPaletteChecks() {
    const theme::PaletteId active = theme::current();
    for (auto& row : this->paletteRows)
        row.card->setValue(row.id == active ? kCheck : "");
}

void SettingsScreen::refreshHaptics() {
    this->hapticsToggle->setValue(onOff(haptics::isEnabled()));

    float cur = 1.0f;
    try {
        nlohmann::ordered_json cfg = fs::parseJsonFile(CONFIG_FILE);
        if (cfg.is_object() && cfg.contains("haptics_strength") && cfg["haptics_strength"].is_number())
            cur = cfg["haptics_strength"].get<float>();
    } catch (...) {}
    this->hapticsStrength->setValue(std::to_string(static_cast<int>(cur * 100.0f + 0.5f)) + "%");
}

void SettingsScreen::refreshBackground() {
    this->backgroundToggle->setValue(onOff(WaveBackground::isEnabled()));
}

}  // namespace ryazhenka

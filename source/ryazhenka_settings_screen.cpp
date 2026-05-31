#include "ryazhenka_settings_screen.hpp"

#include <algorithm>
#include <filesystem>

#include "confirm_page.hpp"
#include "constants.hpp"
#include "download.hpp"
#include "fs.hpp"
#include "ryazhenka_audio.hpp"
#include "ryazhenka_background.hpp"
#include "ryazhenka_banner.hpp"
#include "ryazhenka_catalog.hpp"
#include "ryazhenka_config.hpp"
#include "ryazhenka_haptics.hpp"
#include "ryazhenka_logger.hpp"
#include "ryazhenka_theme.hpp"
#include "ryazhenka_touch.hpp"
#include "progress_event.hpp"
#include "utils.hpp"
#include "worker_page.hpp"

namespace i18n = brls::i18n;
using namespace i18n::literals;

namespace ryazhenka {

namespace {

// Text label for the currently active palette — using a unicode glyph here
// (e.g. ✓) often didn't render through the bundled fonts, leaving the user
// unable to see which palette was selected. Plain localised text is robust.
// The actual string lives in i18n (menus/ryazhenka/settings/selected).

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

// Extracts "OWNER/REPO" from "https://github.com/OWNER/REPO[/...]". Returns
// "" for anything that isn't a github.com URL — Telegram, mirrors, etc.
// Used to wire the ecosystem-list items either to the GitHub release-install
// flow (github) or to the system applet browser (everything else).
std::string githubSlugFromUrl(const std::string& url) {
    static const std::string kPrefix = "https://github.com/";
    if (url.compare(0, kPrefix.size(), kPrefix) != 0)
        return {};
    std::string rest = url.substr(kPrefix.size());
    const auto slash = rest.find('/');
    if (slash == std::string::npos) return {};
    // Trim any trailing path: blob/tree/issues etc.
    const auto next = rest.find('/', slash + 1);
    if (next != std::string::npos)
        rest = rest.substr(0, next);
    return rest;
}

}  // namespace

SettingsScreen::SettingsScreen() {
    // --- Palette ---
    this->addView(new brls::Header("menus/ryazhenka/settings/palette_header"_i18n));

    const theme::PaletteId ids[] = {
        theme::PaletteId::Ryazhenka,
        theme::PaletteId::Standard,
        theme::PaletteId::Slate,
        theme::PaletteId::Cyberpunk,
        theme::PaletteId::Gold,
        theme::PaletteId::Ocean,
    };
    for (theme::PaletteId id : ids) {
        brls::ListItem* card = new brls::ListItem(brls::i18n::getStr(theme::paletteI18nKey(id)));
        card->setHeight(LISTITEM_HEIGHT);
        card->getClickEvent()->subscribe([this, id](brls::View*) {
            // Defensive: applyPalette mutates the live brls::Theme objects;
            // any throw here would otherwise kill the whole process ("Program
            // closed") that the user kept hitting on theme switches.
            try {
                theme::applyPalette(id, /*persist=*/true);
                this->refreshPaletteChecks();
            } catch (...) {
                ryazhenka::log::warn("settings: applyPalette threw");
            }
            try { haptics::success(); } catch (...) {}
        });
        this->addView(card);
        this->paletteRows.push_back({id, card});
    }

    // --- Haptics ---
    this->addView(new brls::Header("menus/ryazhenka/settings/haptics_header"_i18n));

    this->hapticsToggle = new brls::ListItem("menus/ryazhenka/settings/haptics_toggle"_i18n);
    this->hapticsToggle->setHeight(LISTITEM_HEIGHT);
    this->hapticsToggle->getClickEvent()->subscribe([this](brls::View*) {
        bool next = !haptics::isEnabled();
        haptics::setEnabled(next);
        setConfigKey("haptics_enabled", next);
        if (next)
            haptics::click();  // brief confirmation buzz
        this->refreshHaptics();
    });
    this->addView(this->hapticsToggle);

    this->hapticsStrength = new brls::ListItem("menus/ryazhenka/settings/haptics_strength"_i18n);
    this->hapticsStrength->setHeight(LISTITEM_HEIGHT);
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

    this->backgroundToggle = new brls::ListItem("menus/ryazhenka/settings/background_toggle"_i18n);
    this->backgroundToggle->setHeight(LISTITEM_HEIGHT);
    this->backgroundToggle->getClickEvent()->subscribe([this](brls::View*) {
        bool next = !WaveBackground::isEnabled();
        WaveBackground::setEnabled(next);
        setConfigKey("ryazhenka_background", next);
        haptics::click();
        this->refreshBackground();
    });
    this->addView(this->backgroundToggle);

    // Audio toggle restored — pulse() now lazy-inits libnx audout the first
    // time setEnabled(true) is called, so flipping this on no longer needs an
    // app restart to take effect.
    brls::ListItem* audioToggle = new brls::ListItem("menus/ryazhenka/settings/audio_toggle"_i18n);
    audioToggle->setHeight(LISTITEM_HEIGHT);
    audioToggle->setSubLabel("menus/ryazhenka/settings/audio_hint"_i18n);
    audioToggle->getClickEvent()->subscribe([audioToggle](brls::View*) {
        bool next = !audio::isEnabled();
        audio::setEnabled(next);
        setConfigKey("ryazhenka_audio_enabled", next);
        if (next) audio::click();  // preview the new "on" state
        audioToggle->setValue(onOff(next));
    });
    audioToggle->setValue(onOff(audio::isEnabled()));
    this->addView(audioToggle);

    // Touchscreen input — tap the screen to "press A" on the focused item;
    // tap the sidebar area to hop focus there. Default ON.
    brls::ListItem* touchToggle = new brls::ListItem("menus/ryazhenka/settings/touch_toggle"_i18n);
    touchToggle->setHeight(LISTITEM_HEIGHT);
    touchToggle->setSubLabel("menus/ryazhenka/settings/touch_hint"_i18n);
    touchToggle->getClickEvent()->subscribe([touchToggle](brls::View*) {
        bool next = !touch::isEnabled();
        touch::setEnabled(next);
        setConfigKey("ryazhenka_touch_enabled", next);
        touchToggle->setValue(onOff(next));
        haptics::click();
    });
    touchToggle->setValue(onOff(touch::isEnabled()));
    this->addView(touchToggle);

    // --- Caches ---
    this->addView(new brls::Header("menus/ryazhenka/settings/cache_header"_i18n));

    brls::ListItem* clearBanner = new brls::ListItem("menus/ryazhenka/settings/clear_banner"_i18n);
    clearBanner->setHeight(LISTITEM_HEIGHT);
    clearBanner->getClickEvent()->subscribe([](brls::View*) {
        std::error_code ec;
        std::filesystem::remove(kBannerCachePath, ec);
        haptics::success();
    });
    this->addView(clearBanner);

    brls::ListItem* refreshBanner = new brls::ListItem("menus/ryazhenka/settings/refresh_banner"_i18n);
    refreshBanner->setHeight(LISTITEM_HEIGHT);
    refreshBanner->getClickEvent()->subscribe([](brls::View*) {
        // Visible staged frame so the user can see the fetch actually happen
        // (the previous fire-and-forget refreshAsync looked like a no-op when
        // it failed — that was the "banner refresh выдаёт ошибку" report).
        brls::StagedAppletFrame* sf = new brls::StagedAppletFrame();
        sf->setTitle("menus/ryazhenka/settings/refresh_banner"_i18n);
        sf->addStage(new WorkerPage(sf, "menus/common/downloading"_i18n, []() {
            const bool ok = ryazhenka::banner::fetchNow();
            // WorkerPage treats statusCode 0 / >399 as a failure and shows the
            // red error dialog. fetchNow() doesn't touch ProgressEvent, so
            // without this the banner downloaded fine yet the user still got an
            // "HTTP 406" error popup. Report the real outcome instead.
            ProgressEvent::instance().setStatusCode(ok ? 200 : 500);
            if (!ok)
                ryazhenka::log::warn("settings: banner fetchNow returned false");
        }));
        sf->addStage(new ConfirmPage(sf, "menus/common/all_done"_i18n));
        brls::Application::pushView(sf);
        haptics::success();
    });
    this->addView(refreshBanner);

    // Auto-update Ryazhenka pack — checks the current pack tag against the
    // installed one when the toggle is enabled AND the user clicks "Check for
    // updates now". Off by default to honour the "no startup network" rule.
    brls::ListItem* autoUpdateToggle = new brls::ListItem("menus/ryazhenka/settings/autoupdate_toggle"_i18n);
    autoUpdateToggle->setHeight(LISTITEM_HEIGHT);
    autoUpdateToggle->setSubLabel("menus/ryazhenka/settings/autoupdate_hint"_i18n);
    autoUpdateToggle->getClickEvent()->subscribe([autoUpdateToggle](brls::View*) {
        nlohmann::ordered_json cfg;
        try {
            cfg = fs::parseJsonFile(CONFIG_FILE);
            if (!cfg.is_object()) cfg = nlohmann::ordered_json::object();
        } catch (...) { cfg = nlohmann::ordered_json::object(); }
        const bool now = cfg.contains("ryazhenka_autoupdate") && cfg["ryazhenka_autoupdate"].is_boolean()
                             ? cfg["ryazhenka_autoupdate"].get<bool>() : false;
        const bool next = !now;
        cfg["ryazhenka_autoupdate"] = next;
        fs::writeJsonToFile(cfg, CONFIG_FILE);
        autoUpdateToggle->setValue(onOff(next));
        haptics::click();
    });
    {
        nlohmann::ordered_json cfg;
        try { cfg = fs::parseJsonFile(CONFIG_FILE); } catch (...) {}
        const bool on = cfg.is_object() && cfg.contains("ryazhenka_autoupdate")
                        && cfg["ryazhenka_autoupdate"].is_boolean()
                        && cfg["ryazhenka_autoupdate"].get<bool>();
        autoUpdateToggle->setValue(onOff(on));
    }
    this->addView(autoUpdateToggle);

    brls::ListItem* checkUpdates = new brls::ListItem("menus/ryazhenka/settings/check_updates"_i18n);
    checkUpdates->setHeight(LISTITEM_HEIGHT);
    checkUpdates->getClickEvent()->subscribe([](brls::View*) {
        brls::StagedAppletFrame* sf = new brls::StagedAppletFrame();
        sf->setTitle("menus/ryazhenka/settings/check_updates"_i18n);
        sf->addStage(new WorkerPage(sf, "menus/common/downloading"_i18n, []() {
            // banner::fetchNow also refreshes .pack_tag — so the install_pack
            // card on the Tools tab will now show the freshest tag.
            const bool ok = ryazhenka::banner::fetchNow();
            ProgressEvent::instance().setStatusCode(ok ? 200 : 500);
        }));
        sf->addStage(new ConfirmPage(sf, "menus/ryazhenka/settings/check_updates_done"_i18n));
        brls::Application::pushView(sf);
        haptics::success();
    });
    this->addView(checkUpdates);

    // Ecosystem updates — one ListItem per Ryazhenka companion repo. Clicking
    // a GitHub repo runs the standard Confirm → Download → Extract install
    // flow against its latest release; clicking the Telegram link opens the
    // system applet browser. This addresses the user's request to "update
    // all the links from the About tab".
    brls::ListItem* ecosystemUpdates = new brls::ListItem("menus/ryazhenka/settings/ecosystem_updates"_i18n);
    ecosystemUpdates->setHeight(LISTITEM_HEIGHT);
    ecosystemUpdates->getClickEvent()->subscribe([](brls::View*) {
        brls::List* list = new brls::List();
        for (const auto& link : ryazhenka::kEcosystemLinks) {
            const std::string name(link.name);
            const std::string url(link.url);
            brls::ListItem* item = new brls::ListItem(name);
            item->setValue(url);
            item->setHeight(LISTITEM_HEIGHT);
            item->getClickEvent()->subscribe([name, url](brls::View*) {
                const std::string slug = githubSlugFromUrl(url);
                if (slug.empty()) {
                    // Not a GitHub repo (Telegram etc.) — open in the system
                    // browser applet instead of pretending to install it.
                    util::openWebBrowser(url);
                    return;
                }
                // Standard "@latest_asset" install flow: resolve URL, download
                // to CUSTOM_FILENAME, extract at SD root, done.
                brls::StagedAppletFrame* sf = new brls::StagedAppletFrame();
                sf->setTitle(name);
                sf->addStage(new ConfirmPage(sf,
                    std::string("menus/ryazhenka/settings/ecosystem_confirm"_i18n) + "\n\n" + url));
                sf->addStage(new WorkerPage(sf, "menus/common/downloading"_i18n, [slug]() {
                    const std::string asset = download::resolveLatestAssetUrl(slug);
                    if (asset.empty()) {
                        ryazhenka::log::warn("ecosystem: no latest asset for " + slug);
                        return;
                    }
                    util::downloadArchive(asset, contentType::custom);
                }));
                sf->addStage(new WorkerPage(sf, "menus/common/extracting"_i18n, []() {
                    util::extractArchive(contentType::custom);
                }));
                sf->addStage(new ConfirmPage(sf, "menus/common/all_done"_i18n));
                brls::Application::pushView(sf);
            });
            list->addView(item);
        }
        brls::AppletFrame* frame = new brls::AppletFrame(true, true);
        frame->setContentView(list);
        brls::PopupFrame::open("menus/ryazhenka/settings/ecosystem_updates"_i18n,
                               frame, "menus/ryazhenka/settings/ecosystem_hint"_i18n, "");
    });
    this->addView(ecosystemUpdates);

    brls::ListItem* clearSig = new brls::ListItem("menus/ryazhenka/settings/clear_sigpatches"_i18n);
    clearSig->setHeight(LISTITEM_HEIGHT);
    clearSig->getClickEvent()->subscribe([](brls::View*) {
        std::error_code ec;
        std::filesystem::remove(kSigpatchesRemoteCachePath, ec);
        haptics::success();
    });
    this->addView(clearSig);

    brls::ListItem* clearLog = new brls::ListItem("menus/ryazhenka/settings/clear_log"_i18n);
    clearLog->setHeight(LISTITEM_HEIGHT);
    clearLog->getClickEvent()->subscribe([](brls::View*) {
        std::error_code ec;
        std::filesystem::remove(kLogFilePath, ec);
        haptics::success();
    });
    this->addView(clearLog);

    // --- Reset / recovery ---
    this->addView(new brls::Header("menus/ryazhenka/settings/reset_header"_i18n));

    // Emergency: if the user hid every tab via Tools → Hide tabs, this is the
    // only way back. Just delete hide_tabs.json — defaults are "everything
    // visible". Effect on next launch (or after relaunch).
    brls::ListItem* resetHidden = new brls::ListItem("menus/ryazhenka/settings/reset_hidden_tabs"_i18n);
    resetHidden->setHeight(LISTITEM_HEIGHT);
    resetHidden->getClickEvent()->subscribe([](brls::View*) {
        std::error_code ec;
        std::filesystem::remove(HIDE_TABS_JSON, ec);
        haptics::success();
        util::showDialogBoxInfo("menus/ryazhenka/settings/reset_restart"_i18n);
    });
    this->addView(resetHidden);

    // Nuclear option: wipe the whole Ryazhenka config.json (palette, haptics,
    // audio, background). Defaults restored on next launch.
    brls::ListItem* resetConfig = new brls::ListItem("menus/ryazhenka/settings/reset_config"_i18n);
    resetConfig->setHeight(LISTITEM_HEIGHT);
    resetConfig->getClickEvent()->subscribe([](brls::View*) {
        std::error_code ec;
        std::filesystem::remove(CONFIG_FILE, ec);
        haptics::success();
        util::showDialogBoxInfo("menus/ryazhenka/settings/reset_restart"_i18n);
    });
    this->addView(resetConfig);

    // --- About ---
    this->addView(new brls::Header("menus/ryazhenka/settings/about_header"_i18n));
    brls::ListItem* version = new brls::ListItem(std::string(kAppTitleLocalized));
    version->setHeight(LISTITEM_HEIGHT);
    version->setValue(APP_VERSION);
    this->addView(version);
    brls::ListItem* author = new brls::ListItem("menus/ryazhenka/settings/author"_i18n);
    author->setHeight(LISTITEM_HEIGHT);
    author->setValue(std::string(kAppAuthor));
    this->addView(author);

    this->refreshPaletteChecks();
    this->refreshHaptics();
    this->refreshBackground();
}

void SettingsScreen::refreshPaletteChecks() {
    const theme::PaletteId active = theme::current();
    const std::string selected = "menus/ryazhenka/settings/selected"_i18n;
    for (auto& row : this->paletteRows)
        row.card->setValue(row.id == active ? selected : "");
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

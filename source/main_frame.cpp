#include "main_frame.hpp"

#include <fstream>
#include <json.hpp>

#include "about_tab.hpp"
#include "ams_tab.hpp"
#include "download.hpp"
#include "fs.hpp"
#include "list_download_tab.hpp"
#include "ryazhenka_banner.hpp"
#include "ryazhenka_catalog.hpp"
#include "ryazhenka_settings_screen.hpp"
#include "tools_tab.hpp"
#include "utils.hpp"

namespace i18n = brls::i18n;
using namespace i18n::literals;
using json = nlohmann::ordered_json;

namespace {
    constexpr const char AppTitle[] = APP_TITLE;
    constexpr const char AppVersion[] = APP_VERSION;
}  // namespace

MainFrame::MainFrame() : ryazhenka::RyazhenkaTabFrame()
{
    this->setIcon("romfs:/gui_icon.png");
    this->setTitle(AppTitle);

    s64 freeStorage;
    // Skip the GitHub "latest release" tag fetch at ctor time. It used to
    // block the UI thread for up to ~30 s on a flaky network and gave the
    // splash its "бесконечная загрузка" feel. The "update available" cue
    // moves to the Settings tab; the footer just shows the running version
    // and free SD storage.
    this->setFooterText(fmt::format("menus/main/footer_text"_i18n,
                                    AppVersion,
                                    R_SUCCEEDED(fs::getFreeStorageSD(freeStorage)) ? (float)freeStorage / 0x40000000 : -1));

    json hideStatus = fs::parseJsonFile(HIDE_TABS_JSON);
    // Try the on-SD nx-links cache first — pure file read, instant. Only
    // fall back to the blocking HTTPS GET when we have nothing cached
    // (typically first-ever launch).
    nlohmann::ordered_json nxlinks = ryazhenka::catalog::cachedNxLinks();
    if (nxlinks.empty()) {
        download::getRequest(NXLINKS_URL, nxlinks);
        if (!nxlinks.empty())
            ryazhenka::catalog::writeNxLinksCache(nxlinks);
    }

    // Per-release banner: fetch SYNCHRONOUSLY on this (main) thread, exactly
    // like the nx-links GET above, and only when nothing is cached yet. This is
    // the crash-safe way to auto-populate the banner — a detached fetch thread
    // on the launch path is what crashed the app ("Программа закрыта при
    // запуске"), so it is deliberately avoided. Per-release refreshes are done
    // from Settings → "Обновить баннер". Runs before AboutTab is built so the
    // freshly cached image shows on this very launch.
    try {
        if (ryazhenka::banner::cachedPath().empty())
            ryazhenka::banner::fetchNow();
    } catch (...) {
        // best effort — never let a banner hiccup take MainFrame down
    }

    bool erista = util::isErista();

    if (!util::getBoolValue(hideStatus, "about"))
        this->addTab("menus/main/about"_i18n, new AboutTab());

    if (!util::getBoolValue(hideStatus, "atmosphere"))
        this->addTab("menus/main/update_ams"_i18n, new AmsTab_Regular(nxlinks, erista));

    if (!util::getBoolValue(hideStatus, "cfw"))
        this->addTab("menus/main/update_bootloaders"_i18n, new ListDownloadTab(contentType::bootloaders, nxlinks));

    if (!util::getBoolValue(hideStatus, "firmwares"))
        this->addTab("menus/main/download_firmware"_i18n, new ListDownloadTab(contentType::fw, nxlinks));

    if (!util::getBoolValue(hideStatus, "cheats"))
        this->addTab("menus/main/download_cheats"_i18n, new ListDownloadTab(contentType::cheats));

    if (!util::getBoolValue(hideStatus, "custom"))
        this->addTab("menus/main/custom_downloads"_i18n, new AmsTab_Custom(nxlinks, erista));

    if (!util::getBoolValue(hideStatus, "tools"))
        this->addTab("menus/main/tools"_i18n, new ToolsTab("", util::getValueFromKey(nxlinks, "payloads"), erista, hideStatus));

    // Ryazhenka settings — live palette switch, haptics, animated background.
    // ALWAYS shown (regardless of hide_tabs.json) so the user has a way out
    // when every other tab has been hidden. The Settings screen offers a
    // "Reset hidden tabs" card to undo the hide-tabs.json state.
    this->addTab("menus/ryazhenka/settings_tab"_i18n, new ryazhenka::SettingsScreen());

    // Status / dashboard tab removed at the user's request — the metrics
    // sampler never worked reliably on hardware and the tab kept crashing
    // on focus. The StatusTab class is left in the tree (unused) for now.

    this->registerAction("", brls::Key::B, [this] { return true; });
}

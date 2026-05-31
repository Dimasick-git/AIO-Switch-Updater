#include <switch.h>

#include <borealis.hpp>
#include <curl/curl.h>
#include <filesystem>
#include <json.hpp>

#include "constants.hpp"
#include "current_cfw.hpp"
#include "fs.hpp"
#include "main_frame.hpp"
#include "ryazhenka_branding.hpp"
#include "ryazhenka_config.hpp"
#include "ryazhenka_audio.hpp"
#include "ryazhenka_background.hpp"
#include "ryazhenka_banner.hpp"
#include "ryazhenka_catalog.hpp"
#include "ryazhenka_crash_handler.hpp"
#include "ryazhenka_haptics.hpp"
#include "ryazhenka_logger.hpp"
#include "ryazhenka_splash.hpp"
#include "ryazhenka_style.hpp"
#include "ryazhenka_theme.hpp"
#include "ryazhenka_touch.hpp"
// system_info / version_check headers intentionally not included — see body
#include "warning_page.hpp"

namespace i18n = brls::i18n;
using namespace i18n::literals;

//TimeServiceType __nx_time_service_type = TimeServiceType_System;

CFW CurrentCfw::running_cfw;

int main(int argc, char* argv[])
{
    // Init the app with the Ryazhenka style + theme wrapper (custom palettes
    // + narrower sidebar). Application takes ownership of both.
    if (!brls::Application::init(APP_TITLE, new ryazhenka::RyazhenkaStyle(), ryazhenka::theme::makeWrapper())) {
        brls::Logger::error("Unable to init Borealis application");
        return EXIT_FAILURE;
    }

    // Install crash handler AFTER borealis init so its logger is usable.
    try { ryazhenka::crash::install(); } catch (...) {}
    try { ryazhenka::branding::applyBranding(); } catch (...) {}
    // Apply the persisted palette (defaults to Ryazhenka) before any view shows.
    try { ryazhenka::theme::loadAndApplyFromConfig(); } catch (...) {}
    // Animated procedural background behind the whole UI.
    try { brls::Application::setBackground(new ryazhenka::WaveBackground()); } catch (...) {}

    nlohmann::ordered_json languageFile = fs::parseJsonFile(LANGUAGE_JSON);
    if (languageFile.find("language") != languageFile.end())
        i18n::loadTranslations(languageFile["language"]);
    else
        i18n::loadTranslations(); // system locale fallback, same as upstream

        //appletInitializeGamePlayRecording();

        // Setup verbose logging on PC
#ifndef __SWITCH__
    brls::Logger::setLogLevel(brls::LogLevel::DEBUG);
#endif

    setsysInitialize();
    plInitialize(PlServiceType_User);
    nsInitialize();
    socketInitializeDefault();
    nxlinkStdio();
    // curl_global_init must be called once after the socket layer is up and
    // before ANY curl_easy_init() — including calls from background threads.
    curl_global_init(CURL_GLOBAL_ALL);
    pmdmntInitialize();
    pminfoInitialize();
    splInitialize();
    romfsInit();

    CurrentCfw::running_cfw = CurrentCfw::getCFW();

    fs::createTree(CONFIG_PATH);

    brls::Logger::setLogLevel(brls::LogLevel::DEBUG);
    brls::Logger::debug("Start");
    try { ryazhenka::log::init(); } catch (...) {}
    try { ryazhenka::log::info("Ryazhenka Updater started"); } catch (...) {}
    try { ryazhenka::haptics::init(); } catch (...) {}
    try { ryazhenka::audio::init(); } catch (...) {}
    try { ryazhenka::touch::init(); } catch (...) {}

    // Global UI feedback: borealis fires this whenever focus moves between
    // views, so we get a click+buzz on EVERY menu navigation without needing
    // every widget to be a RyazhenkaCard (none of them actually are — the card
    // class was never instantiated, which is why sound/haptics only ever fired
    // from the Settings toggles before). Both calls run on the UI thread and
    // are no-ops when the respective feature is disabled.
    try {
        brls::Application::getGlobalFocusChangeEvent()->subscribe([](brls::View*) {
            try { ryazhenka::audio::focus(); } catch (...) {}
            try { ryazhenka::haptics::focus(); } catch (...) {}
        });
    } catch (...) {}
    // Banner async refresh is intentionally NOT kicked off here. Spawning a
    // detached std::thread that does curl at startup is exactly the pattern
    // that crashed the app on launch and was ripped out in commit ca62519
    // ("rip out every startup-time network/thread/service call"). The cache
    // is now refreshed lazily — banner::makeImage() triggers refreshAsync()
    // itself when the cache is stale, so the first user-visible "About" or
    // "Tools" open does the fetch, never the startup path.
    // sysinfo::collect() and version_check::scheduleBackgroundCheck() are
    // intentionally NOT called here. They each touch the filesystem and the
    // network respectively, and on Switch their failure modes (filesystem_
    // error on libstdc++, mbedtls SSL state on detached curl threads) were
    // showing up as a black screen on launch. They are still in the codebase
    // and can be triggered on-demand from the Status tab if/when the user
    // opens it — by that point the UI is up and a hang/crash there is at
    // worst a single-tab problem, not an app-launch problem.

    if (std::filesystem::exists(HIDDEN_AIO_FILE)) {
        // Show the branded splash first; once the dwell elapses it builds
        // MainFrame and pushes it on top. MainFrame is opaque and its B
        // action is a no-op, so the splash sits invisibly underneath and the
        // user can never navigate back to it.
        //
        // We do NOT popView the splash. brls::Application::popView has a
        // hard guard `if (viewStack.size() <= 1) return;` — it refuses to
        // pop the root view — so calling popView while the splash is the
        // only view on the stack silently no-ops, the callback never fires,
        // and the splash sits there forever. That was the real source of
        // the "бесконечная загрузка" the user reported.
        brls::Application::pushView(new ryazhenka::Splash([] {
            brls::Application::pushView(new MainFrame());
            // NOTE: do NOT kick banner::refreshAsync() (or any detached
            // curl/thread) from here. Even though the splash hands off ~2 s in,
            // this is still the launch path and a detached curl thread here
            // reintroduced the "Программа закрыта при запуске" crash (the exact
            // class of bug ca62519 fixed by ripping every startup-time
            // network/thread/service call out). The banner is instead fetched
            // synchronously, on the main thread, from the MainFrame ctor's
            // existing first-launch network section — safe and crash-free.
        }));
    }
    else {
        brls::Application::pushView(new WarningPage("menus/main/launch_warning"_i18n));
    }

    while (brls::Application::mainLoop())
        ;

    try { ryazhenka::audio::exit(); } catch (...) {}
    try { ryazhenka::haptics::exit(); } catch (...) {}
    // Tell any in-flight banner refresh to stop touching curl before we tear
    // it down. The worker is detached, so we don't join — we just signal and
    // let curl_global_cleanup race-free because the thread will short-circuit
    // before its next curl_easy_init / setopt / perform.
    try { ryazhenka::banner::signalShutdown(); } catch (...) {}
    try { ryazhenka::catalog::signalShutdown(); } catch (...) {}
    romfsExit();
    splExit();
    pminfoExit();
    pmdmntExit();
    curl_global_cleanup();
    socketExit();
    nsExit();
    setsysExit();
    plExit();
    return EXIT_SUCCESS;
}

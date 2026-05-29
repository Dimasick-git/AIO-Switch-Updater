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
#include "ryazhenka_crash_handler.hpp"
#include "ryazhenka_logger.hpp"
#include "ryazhenka_theme.hpp"
// system_info / version_check headers intentionally not included — see body
#include "warning_page.hpp"

namespace i18n = brls::i18n;
using namespace i18n::literals;

//TimeServiceType __nx_time_service_type = TimeServiceType_System;

CFW CurrentCfw::running_cfw;

int main(int argc, char* argv[])
{
    // Init the app with the Ryazhenka theme wrapper (custom palettes). The
    // Application takes ownership of the wrapper.
    if (!brls::Application::init(APP_TITLE, nullptr, ryazhenka::theme::makeWrapper())) {
        brls::Logger::error("Unable to init Borealis application");
        return EXIT_FAILURE;
    }

    // Install crash handler AFTER borealis init so its logger is usable.
    try { ryazhenka::crash::install(); } catch (...) {}
    try { ryazhenka::branding::applyBranding(); } catch (...) {}
    // Apply the persisted palette (defaults to Ryazhenka) before any view shows.
    try { ryazhenka::theme::loadAndApplyFromConfig(); } catch (...) {}

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
    // sysinfo::collect() and version_check::scheduleBackgroundCheck() are
    // intentionally NOT called here. They each touch the filesystem and the
    // network respectively, and on Switch their failure modes (filesystem_
    // error on libstdc++, mbedtls SSL state on detached curl threads) were
    // showing up as a black screen on launch. They are still in the codebase
    // and can be triggered on-demand from the Status tab if/when the user
    // opens it — by that point the UI is up and a hang/crash there is at
    // worst a single-tab problem, not an app-launch problem.

    if (std::filesystem::exists(HIDDEN_AIO_FILE)) {
        brls::Application::pushView(new MainFrame());
    }
    else {
        brls::Application::pushView(new WarningPage("menus/main/launch_warning"_i18n));
    }

    while (brls::Application::mainLoop())
        ;

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

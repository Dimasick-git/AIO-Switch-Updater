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
#include "ryazhenka_system_info.hpp"
#include "ryazhenka_version_check.hpp"
#include "warning_page.hpp"

namespace i18n = brls::i18n;
using namespace i18n::literals;

//TimeServiceType __nx_time_service_type = TimeServiceType_System;

CFW CurrentCfw::running_cfw;

int main(int argc, char* argv[])
{
    // Init the app
    if (!brls::Application::init(APP_TITLE)) {
        brls::Logger::error("Unable to init Borealis application");
        return EXIT_FAILURE;
    }

    // Install crash handler AFTER borealis init so its logger is usable.
    try { ryazhenka::crash::install(); } catch (...) {}
    try { ryazhenka::branding::applyBranding(); } catch (...) {}

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
    try {
        ryazhenka::log::info("sysinfo: " + ryazhenka::sysinfo::formatOneLine(ryazhenka::sysinfo::collect()));
    } catch (...) {
        brls::Logger::error("sysinfo collect threw — skipping startup banner");
    }
    try { ryazhenka::version_check::scheduleBackgroundCheck(); } catch (...) {}

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

#include "tools_tab.hpp"

#include <filesystem>
#include <fstream>

#include "JC_page.hpp"
#include "PC_page.hpp"
#include "app_page.hpp"
#include "cheats_page.hpp"
#include "confirm_page.hpp"
#include "extract.hpp"
#include "fs.hpp"
#include "hide_tabs_page.hpp"
#include "net_page.hpp"
#include "payload_page.hpp"
#include "ryazhenka_backup.hpp"
#include "ryazhenka_card.hpp"
#include "ryazhenka_config.hpp"
#include "ryazhenka_diagnostics.hpp"
#include "ryazhenka_factory_restore.hpp"
#include "ryazhenka_health.hpp"
#include "ryazhenka_logger.hpp"
#include "ryazhenka_status_tab.hpp"
#include "ryazhenka_sysmodule_manager.hpp"
#include "ryazhenka_system_info.hpp"
#include "utils.hpp"
#include "worker_page.hpp"

namespace i18n = brls::i18n;
using namespace i18n::literals;
using json = nlohmann::ordered_json;

namespace {
    constexpr const char AppVersion[] = APP_VERSION;
}

ToolsTab::ToolsTab(const std::string& tag, const nlohmann::ordered_json& payloads, bool erista, const nlohmann::ordered_json& hideStatus) : brls::List()
{
    if (!tag.empty() && tag != AppVersion) {
        auto* updateApp = new ryazhenka::RyazhenkaCard(fmt::format("menus/tools/update_app"_i18n, tag));
        std::string text("menus/tools/dl_app"_i18n + std::string(APP_URL));
        updateApp->getClickEvent()->subscribe([text, tag](brls::View* view) {
            brls::StagedAppletFrame* stagedFrame = new brls::StagedAppletFrame();
            stagedFrame->setTitle("menus/common/updating"_i18n);
            stagedFrame->addStage(
                new ConfirmPage(stagedFrame, text));
            stagedFrame->addStage(
                new WorkerPage(stagedFrame, "menus/common/downloading"_i18n, []() { util::downloadArchive(APP_URL, contentType::app); }));
            stagedFrame->addStage(
                new WorkerPage(stagedFrame, "menus/common/extracting"_i18n, []() { util::extractArchive(contentType::app); }));
            stagedFrame->addStage(
                new ConfirmPage_AppUpdate(stagedFrame, "menus/common/all_done"_i18n));
            brls::Application::pushView(stagedFrame);
        });
        this->addView(updateApp);
    }

    auto* showLog = new ryazhenka::RyazhenkaCard("menus/ryazhenka/show_log"_i18n);
    showLog->getClickEvent()->subscribe([](brls::View* view) {
        const auto lines = ryazhenka::diagnostics::tailLog(200);
        if (lines.empty()) {
            util::showDialogBoxInfo("menus/ryazhenka/log_empty"_i18n);
            return;
        }
        brls::List* list = new brls::List();
        for (const auto& line : lines) {
            brls::ListItem* item = new brls::ListItem(line);
            item->setHeight(LISTITEM_HEIGHT);
            list->addView(item);
        }
        brls::AppletFrame* logView = new brls::AppletFrame(true, true);
        logView->setContentView(list);
        brls::PopupFrame::open("menus/ryazhenka/show_log"_i18n, logView, "", "");
    });

    auto* diagDump = new ryazhenka::RyazhenkaCard("menus/ryazhenka/diag_dump"_i18n);
    diagDump->getClickEvent()->subscribe([](brls::View* view) {
        const std::string path = ryazhenka::diagnostics::writeDumpFile();
        if (path.empty()) {
            util::showDialogBoxInfo("menus/ryazhenka/diag_failed"_i18n);
        } else {
            util::showDialogBoxInfo(std::string("menus/ryazhenka/diag_saved"_i18n) + "\n\n" + path);
        }
    });

    auto* netDiag = new ryazhenka::RyazhenkaCard("menus/ryazhenka/net_diag"_i18n);
    netDiag->getClickEvent()->subscribe([](brls::View* view) {
        const auto results = ryazhenka::diagnostics::runNetworkProbe();
        brls::List* list = new brls::List();
        for (const auto& r : results) {
            std::string label = (r.ok ? "[OK] " : "[FAIL] ") + r.host;
            std::string value = std::to_string(r.http_code) + "  " + std::to_string(r.latency_ms) + " ms";
            brls::ListItem* item = new brls::ListItem(label);
            item->setValue(value);
            item->setHeight(LISTITEM_HEIGHT);
            list->addView(item);
        }
        brls::AppletFrame* diagView = new brls::AppletFrame(true, true);
        diagView->setContentView(list);
        brls::PopupFrame::open("menus/ryazhenka/net_diag"_i18n, diagView, "", "");
    });

    auto* installPack = new ryazhenka::RyazhenkaCard("menus/ryazhenka/install_pack"_i18n);
    installPack->getClickEvent()->subscribe([](brls::View* view) {
        constexpr std::uint64_t kMinFreeBytes = 500ull * 1024ull * 1024ull;
        if (!ryazhenka::sysinfo::hasEnoughFreeSpace(kMinFreeBytes)) {
            util::showDialogBoxInfo("menus/ryazhenka/free_space_low"_i18n);
            return;
        }
        const std::string packUrl(ryazhenka::kPackUrl);
        const std::string confirmText = std::string("menus/ryazhenka/install_pack_confirm"_i18n) + "\n\n" + packUrl;
        brls::StagedAppletFrame* stagedFrame = new brls::StagedAppletFrame();
        stagedFrame->setTitle("menus/ryazhenka/install_pack"_i18n);
        stagedFrame->addStage(new ConfirmPage(stagedFrame, confirmText));
        stagedFrame->addStage(new WorkerPage(stagedFrame, "menus/ryazhenka/backup_before_pack"_i18n, []() {
            if (ryazhenka::backup::isAutoBackupEnabled()) {
                const std::string backup = ryazhenka::backup::snapshotAtmosphere();
                if (backup.empty()) {
                    ryazhenka::log::warn("pack installer: backup skipped or failed; proceeding");
                } else {
                    ryazhenka::log::info("pack installer: backup at " + backup);
                }
            } else {
                ryazhenka::log::info("pack installer: auto_backup_before_pack disabled, skipping");
            }
        }));
        stagedFrame->addStage(new WorkerPage(stagedFrame, "menus/common/downloading"_i18n, [packUrl]() {
            ryazhenka::log::info("pack installer: downloading " + packUrl);
            util::downloadArchive(packUrl, contentType::custom);
        }));
        stagedFrame->addStage(new WorkerPage(stagedFrame, "menus/common/extracting"_i18n, []() {
            ryazhenka::log::info("pack installer: extracting Ryazhenka_AIO.zip");
            util::extractArchive(contentType::custom);
            ryazhenka::health::runAndNotifyIfDegraded();
        }));
        stagedFrame->addStage(new ConfirmPage(stagedFrame, "menus/common/all_done"_i18n));
        brls::Application::pushView(stagedFrame);
    });

    auto* cleanupBackups = new ryazhenka::RyazhenkaCard("menus/ryazhenka/cleanup_backups"_i18n);
    cleanupBackups->getClickEvent()->subscribe([](brls::View* view) {
        const std::size_t removed = ryazhenka::backup::pruneOlderThan(30);
        util::showDialogBoxInfo("menus/common/all_done"_i18n + std::string("\n") + std::to_string(removed));
    });

    auto* sysmoduleManager = new ryazhenka::RyazhenkaCard("menus/ryazhenka/sysmodule_manager"_i18n);
    sysmoduleManager->getClickEvent()->subscribe([](brls::View* view) {
        const auto entries = ryazhenka::sysmodule::list();
        if (entries.empty()) {
            util::showDialogBoxInfo("menus/ryazhenka/sysmodule_empty"_i18n);
            return;
        }
        brls::List* list = new brls::List();
        for (const auto& e : entries) {
            std::string label = e.display_name;
            if (e.critical) label += "  [SYS]";
            label += "  (" + e.title_id + ")";
            brls::ToggleListItem* toggle = new brls::ToggleListItem(
                label,
                e.enabled,
                "",
                "menus/ryazhenka/sysmodule_on"_i18n,
                "menus/ryazhenka/sysmodule_off"_i18n);
            toggle->setHeight(LISTITEM_HEIGHT);
            ryazhenka::sysmodule::Entry captured = e;
            toggle->getClickEvent()->subscribe([captured](brls::View* v) {
                auto* t = dynamic_cast<brls::ToggleListItem*>(v);
                if (!t) return;
                if (captured.critical) {
                    util::showDialogBoxInfo("menus/ryazhenka/sysmodule_critical"_i18n);
                    return;
                }
                const bool desired = t->getToggleState();
                const bool ok = ryazhenka::sysmodule::setEnabled(captured, desired);
                if (!ok) {
                    util::showDialogBoxInfo("menus/ryazhenka/sysmodule_failed"_i18n);
                }
                ryazhenka::health::runAndNotifyIfDegraded();
            });
            list->addView(toggle);
        }
        brls::AppletFrame* sysView = new brls::AppletFrame(true, true);
        sysView->setContentView(list);
        brls::PopupFrame::open("menus/ryazhenka/sysmodule_manager"_i18n, sysView, "menus/ryazhenka/sysmodule_hint"_i18n, "");
    });

    auto* openDashboard = new ryazhenka::RyazhenkaCard("menus/ryazhenka/open_dashboard"_i18n);
    openDashboard->getClickEvent()->subscribe([](brls::View* view) {
        brls::Application::pushView(new ryazhenka::StatusTab());
    });

    auto* factoryRestore = new ryazhenka::RyazhenkaCard("menus/ryazhenka/factory_restore"_i18n);
    factoryRestore->getClickEvent()->subscribe([](brls::View* view) {
        const auto backups = ryazhenka::restore::listBackups(50);
        if (backups.empty()) {
            util::showDialogBoxInfo("menus/ryazhenka/restore_no_backups"_i18n);
            return;
        }
        brls::List* list = new brls::List();
        for (const auto& b : backups) {
            const std::string label = ryazhenka::restore::formatLabel(b);
            const std::string value =
                std::to_string(b.size_bytes / (1024ull * 1024ull)) + " MiB · " +
                std::to_string(b.file_count) + " file(s)";
            brls::ListItem* item = new brls::ListItem(label);
            item->setValue(value);
            item->setHeight(LISTITEM_HEIGHT);
            ryazhenka::restore::BackupEntry captured = b;
            item->getClickEvent()->subscribe([captured](brls::View* v) {
                const std::string confirmText =
                    std::string("menus/ryazhenka/restore_confirm"_i18n) +
                    "\n\n" + captured.path.string();
                brls::StagedAppletFrame* stagedFrame = new brls::StagedAppletFrame();
                stagedFrame->setTitle("menus/ryazhenka/factory_restore"_i18n);
                stagedFrame->addStage(new ConfirmPage(stagedFrame, confirmText));
                stagedFrame->addStage(new WorkerPage(
                    stagedFrame,
                    "menus/ryazhenka/restore_progress"_i18n,
                    [captured]() {
                        const bool ok = ryazhenka::restore::restoreFrom(
                            captured,
                            [](ryazhenka::restore::Stage, std::size_t, std::size_t) {});
                        if (!ok) ryazhenka::log::warn("restore: failed");
                        ryazhenka::health::runAndNotifyIfDegraded();
                    }));
                stagedFrame->addStage(new ConfirmPage(stagedFrame, "menus/ryazhenka/restore_done"_i18n));
                brls::Application::pushView(stagedFrame);
            });
            list->addView(item);
        }
        brls::AppletFrame* restoreView = new brls::AppletFrame(true, true);
        restoreView->setContentView(list);
        brls::PopupFrame::open("menus/ryazhenka/factory_restore"_i18n, restoreView, "menus/ryazhenka/restore_hint"_i18n, "");
    });

    auto* cheats = new ryazhenka::RyazhenkaCard("menus/tools/cheats"_i18n);
    cheats->getClickEvent()->subscribe([](brls::View* view) {
        brls::PopupFrame::open("menus/cheats/menu"_i18n, new CheatsPage(), "", "");
    });

    auto* outdatedTitles = new ryazhenka::RyazhenkaCard("menus/tools/outdated_titles"_i18n);
    outdatedTitles->getClickEvent()->subscribe([](brls::View* view) {
        brls::PopupFrame::open("menus/tools/outdated_titles"_i18n, new AppPage_OutdatedTitles(), "menus/tools/outdated_titles_desc"_i18n, "");
    });

    auto* JCcolor = new ryazhenka::RyazhenkaCard("menus/tools/joy_cons"_i18n);
    JCcolor->getClickEvent()->subscribe([](brls::View* view) {
        brls::Application::pushView(new JCPage());
    });

    auto* PCcolor = new ryazhenka::RyazhenkaCard("menus/tools/pro_cons"_i18n);
    PCcolor->getClickEvent()->subscribe([](brls::View* view) {
        brls::Application::pushView(new PCPage());
    });

    auto* rebootPayload = new ryazhenka::RyazhenkaCard("menus/tools/inject_payloads"_i18n);
    rebootPayload->getClickEvent()->subscribe([](brls::View* view) {
        brls::PopupFrame::open("menus/tools/inject_payloads"_i18n, new PayloadPage(), "", "");
    });

    auto* netSettings = new ryazhenka::RyazhenkaCard("menus/tools/internet_settings"_i18n);
    netSettings->getClickEvent()->subscribe([](brls::View* view) {
        brls::PopupFrame::open("menus/tools/internet_settings"_i18n, new NetPage(), "", "");
    });

    auto* browser = new ryazhenka::RyazhenkaCard("menus/tools/browser"_i18n);
    browser->getClickEvent()->subscribe([](brls::View* view) {
        std::string url;
        if (brls::Swkbd::openForText([&url](std::string text) { url = text; }, "cheatslips.com e-mail", "", 256, "https://duckduckgo.com", 0, "Submit", "https://website.tld")) {
            util::openWebBrowser(url);
        }
    });

    auto* move = new ryazhenka::RyazhenkaCard("menus/tools/batch_copy"_i18n);
    move->getClickEvent()->subscribe([](brls::View* view) {
        chdir("/");
        std::string error = "";
        if (std::filesystem::exists(COPY_FILES_TXT)) {
            error = fs::copyFiles(COPY_FILES_TXT);
        }
        else {
            error = "menus/tools/batch_copy_config_not_found"_i18n;
        }
        util::showDialogBoxInfo(error);
    });

    auto* cleanUp = new ryazhenka::RyazhenkaCard("menus/tools/clean_up"_i18n);
    cleanUp->getClickEvent()->subscribe([](brls::View* view) {
        std::filesystem::remove(AMS_FILENAME);
        std::filesystem::remove(APP_FILENAME);
        std::filesystem::remove(FIRMWARE_FILENAME);
        std::filesystem::remove(CHEATS_FILENAME);
        std::filesystem::remove(BOOTLOADER_FILENAME);
        std::filesystem::remove(CHEATS_VERSION);
        std::filesystem::remove(CUSTOM_FILENAME);
        fs::removeDir(AMS_DIRECTORY_PATH);
        fs::removeDir(SEPT_DIRECTORY_PATH);
        fs::removeDir(FW_DIRECTORY_PATH);
        util::showDialogBoxInfo("menus/common/all_done"_i18n);
    });

    auto* language = new ryazhenka::RyazhenkaCard("menus/tools/language"_i18n);
    language->getClickEvent()->subscribe([](brls::View* view) {
        std::vector<std::pair<std::string, std::string>> languages{
            std::make_pair("American English ({})", "en-US"),
            std::make_pair("日本語 ({})", "ja"),
            std::make_pair("Français ({})", "fr"),
            std::make_pair("Deutsch ({})", "de"),
            std::make_pair("Italiano ({})", "it"),
            std::make_pair("Español ({})", "es"),
            std::make_pair("Português ({})", "pt-BR"),
            std::make_pair("Nederlands ({})", "nl"),
            std::make_pair("Русский ({})", "ru"),
            std::make_pair("Română ({})", "ro"),
            std::make_pair("한국어 ({})", "ko"),
            std::make_pair("Polski ({})", "pl"),
            std::make_pair("简体中文 ({})", "zh-CN"),
            std::make_pair("繁體中文 ({})", "zh-TW"),
            std::make_pair("English (Great Britain) ({})", "en-GB"),
            std::make_pair("Français (Canada) ({})", "fr-CA"),
            std::make_pair("Español (Latinoamérica) ({})", "es-419"),
            std::make_pair("Português brasileiro ({})", "pt-BR"),
            std::make_pair("Traditional Chinese ({})", "zh-Hant"),
            std::make_pair("Simplified Chinese ({})", "zh-Hans")};
        brls::AppletFrame* appView = new brls::AppletFrame(true, true);
        brls::List* list = new brls::List();
        brls::ListItem* listItem;
        listItem = new brls::ListItem(fmt::format("System Default ({})", i18n::getCurrentLocale()));
        listItem->registerAction("menus/tools/language"_i18n, brls::Key::A, [] {
            std::filesystem::remove(LANGUAGE_JSON);
            brls::Application::quit();
            return true;
        });
        list->addView(listItem);
        for (auto& language : languages) {
            if (std::filesystem::exists(fmt::format(LOCALISATION_FILE, language.second))) {
                listItem = new brls::ListItem(fmt::format(language.first, language.second));
                listItem->registerAction("menus/tools/language"_i18n, brls::Key::A, [language] {
                    json updatedLanguage = json::object();
                    updatedLanguage["language"] = language.second;
                    std::ofstream out(LANGUAGE_JSON);
                    out << updatedLanguage.dump();
                    brls::Application::quit();
                    return true;
                });
                list->addView(listItem);
            }
        }
        appView->setContentView(list);
        brls::PopupFrame::open("menus/tools/language"_i18n, appView, "", "");
    });

    auto* hideTabs = new ryazhenka::RyazhenkaCard("menus/tools/hide_tabs"_i18n);
    hideTabs->getClickEvent()->subscribe([](brls::View* view) {
        brls::PopupFrame::open("menus/tools/hide_tabs"_i18n, new HideTabsPage(), "", "");
    });

    auto* changelog = new ryazhenka::RyazhenkaCard("menus/tools/changelog"_i18n);
    changelog->getClickEvent()->subscribe([](brls::View* view) {
        util::openWebBrowser(CHANGELOG_URL);
    });

    if (!util::getBoolValue(hideStatus, "cheats")) this->addView(cheats);
    this->addView(installPack);
    this->addView(sysmoduleManager);
    this->addView(factoryRestore);
    this->addView(openDashboard);
    this->addView(cleanupBackups);
    this->addView(showLog);
    this->addView(diagDump);
    this->addView(netDiag);
    if (!util::getBoolValue(hideStatus, "outdatedtitles")) this->addView(outdatedTitles);
    if (!util::getBoolValue(hideStatus, "jccolor")) this->addView(JCcolor);
    if (!util::getBoolValue(hideStatus, "pccolor")) this->addView(PCcolor);
    if (erista && !util::getBoolValue(hideStatus, "rebootpayload")) this->addView(rebootPayload);
    if (!util::getBoolValue(hideStatus, "netsettings")) this->addView(netSettings);
    if (!util::getBoolValue(hideStatus, "browser")) this->addView(browser);
    if (!util::getBoolValue(hideStatus, "move")) this->addView(move);
    if (!util::getBoolValue(hideStatus, "cleanup")) this->addView(cleanUp);
    if (!util::getBoolValue(hideStatus, "language")) this->addView(language);
    this->addView(hideTabs);
    this->addView(changelog);
}

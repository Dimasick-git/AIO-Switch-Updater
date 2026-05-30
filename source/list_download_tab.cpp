#include "list_download_tab.hpp"


#include <filesystem>
#include <fstream>
#include <string>

#include "app_page.hpp"
#include "confirm_page.hpp"
#include "current_cfw.hpp"
#include "dialogue_page.hpp"
#include "download.hpp"
#include "extract.hpp"
#include "fs.hpp"
#include "progress_event.hpp"
#include "utils.hpp"
#include "worker_page.hpp"

namespace i18n = brls::i18n;
using namespace i18n::literals;

ListDownloadTab::ListDownloadTab(const contentType type, const nlohmann::ordered_json& nxlinks) : brls::List(), type(type), nxlinks(nxlinks)
{
    this->setDescription();

    this->createList();

    if (this->type == contentType::cheats) {
        brls::Label* cheatsLabel = new brls::Label(
            brls::LabelStyle::DESCRIPTION,
            "menus/cheats/cheats_label"_i18n,
            true);
        this->addView(cheatsLabel);
        this->createGbatempItem();
        this->createGfxItem();
        this->createCheatSlipItem();
    }

    if (this->type == contentType::bootloaders) {
        this->setDescription(contentType::hekate_ipl);
        this->createList(contentType::hekate_ipl);
        this->setDescription(contentType::payloads);
        this->createList(contentType::payloads);
    }
}

void ListDownloadTab::createList()
{
    ListDownloadTab::createList(this->type);
}

void ListDownloadTab::createList(contentType type)
{
    std::vector<std::pair<std::string, std::string>> links;
    if (type == contentType::cheats && this->newCheatsVer != "") {
        links.push_back(std::make_pair(fmt::format("menus/main/get_cheats"_i18n, this->newCheatsVer), CurrentCfw::running_cfw == CFW::sxos ? CHEATS_URL_TITLES : CHEATS_URL_CONTENTS));
        links.push_back(std::make_pair("menus/main/get_cheats_gfx"_i18n, CurrentCfw::running_cfw == CFW::sxos ? GFX_CHEATS_URL_TITLES : GFX_CHEATS_URL_CONTENTS));
    }
    else
        links = download::getLinksFromJson(util::getValueFromKey(this->nxlinks, contentTypeNames[(int)type].data()));

    if (links.size()) {
        for (const auto& link : links) {
            const std::string title = link.first;
            const std::string url = link.second;
            const std::string text("menus/common/download"_i18n + link.first + "menus/common/from"_i18n + url);
            listItem = new brls::ListItem(link.first);
            listItem->setHeight(LISTITEM_HEIGHT);
            listItem->getClickEvent()->subscribe([this, type, text, url, title](brls::View* view) {
                // Three URL markers are recognised in nx-links.json. They all
                // resolve at click time (not at MainFrame ctor) so launch is
                // never blocked on a network call.
                //
                //   @latest_asset:OWNER/REPO
                //     → query api.github.com/.../releases/latest, take the
                //       first .zip asset (fallback assets[0]). Standard
                //       extract-to-SD-root flow.
                //
                //   @download_to:<DEST_PATH>:<REST>
                //     → REST may be another marker or a direct URL. Download
                //       the resolved URL to DEST_PATH (parent dirs created
                //       automatically), no extract. Used for single-file
                //       drops like /switch/.overlays/foo.ovl or
                //       /switch/RyazhaAI/ryazha-ai.nro.
                //
                //   @reorg_ppsspp:OWNER/REPO
                //     → resolve like @latest_asset, do the standard extract,
                //       then move /PPSSPP_GL.nro → /switch/PPSSPP_GL.nro and
                //       /assets → /switch/ppsspp/assets so the emulator
                //       lands where it expects.
                std::string resolved = url;
                std::string download_to;    // non-empty → skip extract, single-file path
                bool ppsspp_reorg = false;

                if (resolved.compare(0, 13, "@download_to:") == 0) {
                    const std::string rest = resolved.substr(13);
                    const auto sep = rest.find(':');
                    if (sep == std::string::npos) {
                        util::showDialogBoxInfo("Malformed @download_to: marker in nx-links.json");
                        return;
                    }
                    download_to = rest.substr(0, sep);
                    resolved = rest.substr(sep + 1);
                }
                if (resolved.compare(0, 14, "@reorg_ppsspp:") == 0) {
                    ppsspp_reorg = true;
                    resolved = "@latest_asset:" + resolved.substr(14);
                }
                if (resolved.compare(0, 14, "@latest_asset:") == 0) {
                    const std::string slug = resolved.substr(14);
                    resolved = download::resolveLatestAssetUrl(slug);
                    if (resolved.empty()) {
                        util::showDialogBoxInfo(fmt::format("menus/errors/no_internet_url"_i18n, slug));
                        return;
                    }
                }

                brls::StagedAppletFrame* stagedFrame = new brls::StagedAppletFrame();
                stagedFrame->setTitle(fmt::format("menus/main/getting"_i18n, contentTypeNames[(int)type].data()));
                stagedFrame->addStage(new ConfirmPage(stagedFrame, text));
                if (type == contentType::fw) {
                    std::string contentsPath = util::getContentsPath();
                    for (const auto& tid : {"0100000000001000", "0100000000001007", "0100000000001013"}) {
                        if (std::filesystem::exists(contentsPath + tid) && !std::filesystem::is_empty(contentsPath + tid)) {
                            stagedFrame->addStage(new DialoguePage_confirm(stagedFrame, "menus/main/theme_warning"_i18n));
                        }
                    }
                }

                if (!download_to.empty()) {
                    // @download_to: single-file flow, no extract. Used for
                    // .ovl drops into /switch/.overlays/ and for the
                    // RyazhaAI .nro into /switch/RyazhaAI/.
                    fs::createTree(std::filesystem::path(download_to).parent_path().string() + "/");
                    stagedFrame->addStage(new WorkerPage(stagedFrame, "menus/common/downloading"_i18n, [resolved, download_to]() {
                        long sc = download::downloadFile(resolved, download_to, OFF);
                        ProgressEvent::instance().setStatusCode(sc);
                    }));
                }
                else if (type != contentType::payloads && type != contentType::hekate_ipl) {
                    if (type != contentType::cheats || (this->newCheatsVer != this->currentCheatsVer && this->newCheatsVer != "offline")) {
                        stagedFrame->addStage(new WorkerPage(stagedFrame, "menus/common/downloading"_i18n, [this, type, resolved]() { util::downloadArchive(resolved, type); }));
                    }
                    stagedFrame->addStage(new WorkerPage(stagedFrame, "menus/common/extracting"_i18n, [this, type]() { util::extractArchive(type, this->newCheatsVer); }));
                    if (ppsspp_reorg) {
                        // Upstream PPSSPP.zip extracts /PPSSPP_GL.nro and
                        // /assets/* at SD root; the emulator expects them at
                        // /switch/PPSSPP_GL.nro and /switch/ppsspp/assets/.
                        // Move them after extract; create parents if missing.
                        stagedFrame->addStage(new WorkerPage(stagedFrame, "menus/common/extracting"_i18n, []() {
                            std::error_code ec;
                            std::filesystem::create_directories("/switch", ec);
                            std::filesystem::create_directories("/switch/ppsspp", ec);
                            if (std::filesystem::exists("/PPSSPP_GL.nro", ec)) {
                                std::filesystem::rename("/PPSSPP_GL.nro", "/switch/PPSSPP_GL.nro", ec);
                            }
                            if (std::filesystem::is_directory("/assets", ec)) {
                                std::filesystem::rename("/assets", "/switch/ppsspp/assets", ec);
                            }
                            ProgressEvent::instance().setStatusCode(200);
                        }));
                    }
                }
                else if (type == contentType::payloads) {
                    fs::createTree(BOOTLOADER_PL_PATH);
                    std::string path = std::string(BOOTLOADER_PL_PATH) + title;
                    stagedFrame->addStage(new WorkerPage(stagedFrame, "menus/common/downloading"_i18n, [resolved, path]() {
                        long sc = download::downloadFile(resolved, path, OFF);
                        ProgressEvent::instance().setStatusCode(sc);
                    }));
                }
                else if (type == contentType::hekate_ipl) {
                    fs::createTree(BOOTLOADER_PATH);
                    std::string path = std::string(BOOTLOADER_PATH) + title;
                    stagedFrame->addStage(new WorkerPage(stagedFrame, "menus/common/downloading"_i18n, [resolved, path]() {
                        long sc = download::downloadFile(resolved, path, OFF);
                        ProgressEvent::instance().setStatusCode(sc);
                    }));
                }

                std::string doneMsg = "menus/common/all_done"_i18n;
                if (type == contentType::fw && std::filesystem::exists(DAYBREAK_PATH)) {
                        stagedFrame->addStage(new DialoguePage_fw(stagedFrame, doneMsg));
                }
                else {
                    stagedFrame->addStage(new ConfirmPage_Done(stagedFrame, doneMsg));
                }
                brls::Application::pushView(stagedFrame);
            });
            this->addView(listItem);
        }
    }
    else {
        this->displayNotFound();
    }
}

void ListDownloadTab::displayNotFound()
{
    brls::Label* notFound = new brls::Label(
        brls::LabelStyle::SMALL,
        "menus/main/links_not_found"_i18n,
        true);
    notFound->setHorizontalAlign(NVG_ALIGN_CENTER);
    this->addView(notFound);
}

void ListDownloadTab::setDescription()
{
    this->setDescription(this->type);
}

void ListDownloadTab::setDescription(contentType type)
{
    brls::Label* description = new brls::Label(brls::LabelStyle::DESCRIPTION, "", true);

    switch (type) {
        case contentType::fw: {
            SetSysFirmwareVersion ver;
            description->setText(fmt::format("{}{}", "menus/main/firmware_text"_i18n, R_SUCCEEDED(setsysGetFirmwareVersion(&ver)) ? ver.display_version : "menus/main/not_found"_i18n));
            break;
        }
        case contentType::bootloaders:
            description->setText(
                "menus/main/bootloaders_text"_i18n);
            break;
        case contentType::cheats:
            // Skip the blocking HTTPS GET to CHEATS_URL_VERSION at ctor time.
            // HamletDuFromage's switch-cheats-db has gone stale (1.5+ months
            // no updates) and the call can hang the whole UI for minutes on a
            // flaky connection — that's the "бесконечная загрузка" the user
            // saw on the splash. Show whatever's cached on SD and use a
            // sentinel for newCheatsVer that satisfies the createList /
            // download-stage guards (non-empty, not "offline", and not equal
            // to currentCheatsVer) so clicking the cheats card still downloads
            // a fresh archive.
            this->currentCheatsVer = util::readFile(CHEATS_VERSION);
            this->newCheatsVer = "latest";
            description->setText("menus/main/cheats_text"_i18n + this->currentCheatsVer);
            break;
        case contentType::payloads:
            description->setText(fmt::format("menus/main/payloads_label"_i18n, BOOTLOADER_PL_PATH));
            break;
        case contentType::hekate_ipl:
            description->setText("menus/main/hekate_ipl_label"_i18n);
            break;
        default:
            break;
    }

    this->addView(description);
}

void ListDownloadTab::createCheatSlipItem()
{
    brls::ListItem* cheatslipsItem = new brls::ListItem("menus/cheats/get_cheatslips"_i18n);
    cheatslipsItem->setHeight(LISTITEM_HEIGHT);
    cheatslipsItem->getClickEvent()->subscribe([](brls::View* view) {
        if (std::filesystem::exists(TOKEN_PATH)) {
            brls::Application::pushView(new AppPage_CheatSlips());
        }
        else {
            std::string usr, pwd;
            // Result rc = swkbdCreate(&kbd, 0);
            brls::Swkbd::openForText([&usr](std::string text) { usr = text; }, "cheatslips.com e-mail", "", 64, "", 0, "Submit", "cheatslips.com e-mail");
            brls::Swkbd::openForText([&pwd](std::string text) { pwd = text; }, "cheatslips.com password", "", 64, "", 0, "Submit", "cheatslips.com password", true);
            std::string body = "{\"email\":\"" + std::string(usr) + "\",\"password\":\"" + std::string(pwd) + "\"}";
            nlohmann::ordered_json token;
            download::getRequest(CHEATSLIPS_TOKEN_URL, token,
                                 {"Accept: application/json",
                                  "Content-Type: application/json",
                                  "charset: utf-8"},
                                 body);
            if (token.find("token") != token.end()) {
                std::ofstream tokenFile(TOKEN_PATH);
                tokenFile << token.dump();
                tokenFile.close();
                brls::Application::pushView(new AppPage_CheatSlips());
            }
            else {
                util::showDialogBoxInfo("menus/cheats/cheatslips_wrong_id"_i18n + "\n" + "menus/cheats/kb_error"_i18n);
            }
        }
        return true;
    });
    this->addView(cheatslipsItem);
}

void ListDownloadTab::createGbatempItem()
{
    brls::ListItem* gbatempItem = new brls::ListItem("menus/cheats/get_gbatemp"_i18n);
    gbatempItem->setHeight(LISTITEM_HEIGHT);
    gbatempItem->getClickEvent()->subscribe([](brls::View* view) {
        brls::Application::pushView(new AppPage_Gbatemp());
        return true;
    });
    this->addView(gbatempItem);
}

void ListDownloadTab::createGfxItem()
{
    brls::ListItem* gfxItem = new brls::ListItem("menus/cheats/get_gfx"_i18n);
    gfxItem->setHeight(LISTITEM_HEIGHT);
    gfxItem->getClickEvent()->subscribe([](brls::View* view) {
        brls::Application::pushView(new AppPage_Gfx());
        return true;
    });
    this->addView(gfxItem);
}
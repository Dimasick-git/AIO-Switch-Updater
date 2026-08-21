#include "extract.hpp"

#include <dirent.h>
#include <minizip/unzip.h>

#include <algorithm>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iterator>
#include <ranges>
#include <set>
#include <sstream>
#include <string>
#include <thread>
#include <vector>

#include "current_cfw.hpp"
#include "download.hpp"
#include "fs.hpp"
#include "main_frame.hpp"
#include "progress_event.hpp"
#include "utils.hpp"

namespace i18n = brls::i18n;
using namespace i18n::literals;

constexpr size_t WRITE_BUFFER_SIZE = 0x10000;

namespace extract {

    namespace {
        bool caselessCompare(const std::string& a, const std::string& b)
        {
            return strcasecmp(a.c_str(), b.c_str()) == 0;
        }

        bool getUncompressedSize(const std::string& archivePath, s64& size)
        {
            size = 0;
            unzFile zfile = unzOpen(archivePath.c_str());
            if (zfile == nullptr)
                return false;

            unz_global_info gi;
            if (unzGetGlobalInfo(zfile, &gi) != UNZ_OK) {
                unzClose(zfile);
                return false;
            }

            for (uLong i = 0; i < gi.number_entry; ++i) {
                unz_file_info fi;
                if (unzGetCurrentFileInfo(zfile, &fi, NULL, 0, NULL, 0, NULL, 0) != UNZ_OK) {
                    unzClose(zfile);
                    return false;
                }
                size += static_cast<s64>(fi.uncompressed_size);
                if (i + 1 < gi.number_entry && unzGoToNextFile(zfile) != UNZ_OK) {
                    unzClose(zfile);
                    return false;
                }
            }
            unzClose(zfile);
            return true;
        }

        bool ensureAvailableStorage(const std::string& archivePath)
        {
            s64 uncompressedSize = 0;
            if (!getUncompressedSize(archivePath, uncompressedSize)) {
                brls::Logger::error("Cannot inspect archive {} before extraction", archivePath);
                return false;
            }

            s64 archiveSize = 0;
            try {
                archiveSize = static_cast<s64>(std::filesystem::file_size(archivePath));
            }
            catch (const std::filesystem::filesystem_error&) {
                brls::Logger::error("Cannot get archive size for {}", archivePath);
                return false;
            }

            s64 freeStorage = 0;
            if (R_FAILED(fs::getFreeStorageSD(freeStorage))) {
                brls::Logger::error("Cannot query free space on SD card");
                return false;
            }

            // The archive remains on the SD card while its payload is written.
            constexpr s64 safetyMargin = 4 * 1024 * 1024;
            const s64 requiredStorage = uncompressedSize + archiveSize + safetyMargin;
            brls::Logger::info("Archive {} requires {} bytes (payload {}, archive {}, margin {}). Available: {}",
                archivePath, requiredStorage, uncompressedSize, archiveSize, safetyMargin, freeStorage);
            return freeStorage >= requiredStorage;
        }

        bool extractEntry(const std::string& filename, unzFile& zfile)
        {
            if (filename.empty())
                return false;
            if (filename.back() == '/') {
                fs::createTree(filename);
                return true;
            }

            // ZIP writers are not required to list every parent directory.
            fs::createTree(filename);
            void* buf = malloc(WRITE_BUFFER_SIZE);
            if (buf == nullptr) {
                brls::Logger::error("Out of memory while extracting {}", filename);
                return false;
            }

            FILE* outfile = fopen(filename.c_str(), "wb");
            if (outfile == nullptr) {
                brls::Logger::error("Cannot open {} for writing", filename);
                free(buf);
                return false;
            }

            bool success = true;
            for (;;) {
                const int readSize = unzReadCurrentFile(zfile, buf, WRITE_BUFFER_SIZE);
                if (readSize == 0)
                    break;
                if (readSize < 0 || fwrite(buf, 1, static_cast<size_t>(readSize), outfile) != static_cast<size_t>(readSize)) {
                    brls::Logger::error("Write or ZIP read failed for {}", filename);
                    success = false;
                    break;
                }
            }

            free(buf);
            if (fclose(outfile) != 0) {
                brls::Logger::error("Cannot finish writing {}", filename);
                success = false;
            }
            return success;
        }
    }  // namespace

    bool extract(const std::string& archivePath, const std::string& workingPath, bool preserveInis, std::function<void()> func)
    {
        if (!ensureAvailableStorage(archivePath)) {
            brls::Logger::error("Insufficient storage or unreadable archive: {}", archivePath);
            ProgressEvent::instance().setStatusCode(507);
            return false;
        }

        unzFile zfile = unzOpen(archivePath.c_str());
        if (zfile == nullptr) {
            brls::Logger::error("Cannot open ZIP archive {}", archivePath);
            ProgressEvent::instance().setStatusCode(406);
            return false;
        }
        unz_global_info gi;
        if (unzGetGlobalInfo(zfile, &gi) != UNZ_OK) {
            brls::Logger::error("Cannot read ZIP directory from {}", archivePath);
            unzClose(zfile);
            ProgressEvent::instance().setStatusCode(406);
            return false;
        }

        ProgressEvent::instance().setTotalSteps(gi.number_entry);
        ProgressEvent::instance().setStep(0);

        std::set<std::string> ignoreList = fs::readLineByLine(FILES_IGNORE);
        std::string appPath = util::getAppPath();

        for (uLong i = 0; i < gi.number_entry; ++i) {
            char szFilename[0x301] = "";
            if (unzOpenCurrentFile(zfile) != UNZ_OK || unzGetCurrentFileInfo(zfile, NULL, szFilename, sizeof(szFilename), NULL, 0, NULL, 0) != UNZ_OK) {
                brls::Logger::error("Cannot read ZIP entry {} from {}", i, archivePath);
                unzClose(zfile);
                ProgressEvent::instance().setStatusCode(406);
                return false;
            }
            std::string filename = workingPath + szFilename;

            if (ProgressEvent::instance().getInterupt()) {
                unzCloseCurrentFile(zfile);
                break;
            }
            if (appPath != filename) {
                if ((preserveInis == true && filename.substr(filename.length() - 4) == ".ini") || std::find_if(ignoreList.begin(), ignoreList.end(), [&filename](std::string ignored) {
                                                                                                                    u8 res = filename.find(ignored);
                                                                                                                    return (res == 0 || res == 1); }) != ignoreList.end()) {
                    if (!std::filesystem::exists(filename) && !extractEntry(filename, zfile)) {
                        unzCloseCurrentFile(zfile);
                        unzClose(zfile);
                        ProgressEvent::instance().setStatusCode(507);
                        return false;
                    }
                }
                else {
                    if ((filename == "/atmosphere/package3") || (filename == "/atmosphere/stratosphere.romfs")) {
                        if (!extractEntry(filename + ".aio", zfile)) {
                            unzCloseCurrentFile(zfile);
                            unzClose(zfile);
                            ProgressEvent::instance().setStatusCode(507);
                            return false;
                        }
                    }
                    else {
                        if (!extractEntry(filename, zfile)) {
                            unzCloseCurrentFile(zfile);
                            unzClose(zfile);
                            ProgressEvent::instance().setStatusCode(507);
                            return false;
                        }
                        if (filename.substr(0, 14) == "/hekate_ctcaer") {
                            fs::copyFile(filename, UPDATE_BIN_PATH);
                            if (CurrentCfw::running_cfw == CFW::ams && util::showDialogBoxBlocking(fmt::format("menus/utils/set_hekate_reboot_payload"_i18n, UPDATE_BIN_PATH, REBOOT_PAYLOAD_PATH), "menus/common/yes"_i18n, "menus/common/no"_i18n) == 0) {
                                fs::copyFile(UPDATE_BIN_PATH, REBOOT_PAYLOAD_PATH);
                            }
                        }
                    }
                }
            }
            ProgressEvent::instance().setStep(i);
            unzCloseCurrentFile(zfile);
            unzGoToNextFile(zfile);
        }
        unzClose(zfile);
        ProgressEvent::instance().setStep(ProgressEvent::instance().getMax());
        func();
        return true;
    }

    std::vector<std::string> getInstalledTitlesNs()
    {
        std::vector<std::string> titles;

        NsApplicationRecord* records = new NsApplicationRecord[MaxTitleCount]();
        NsApplicationControlData* controlData = NULL;

        s32 recordCount = 0;
        u64 controlSize = 0;

        if (R_SUCCEEDED(nsListApplicationRecord(records, MaxTitleCount, 0, &recordCount))) {
            for (s32 i = 0; i < recordCount; i++) {
                controlSize = 0;
                free(controlData);
                controlData = (NsApplicationControlData*)malloc(sizeof(NsApplicationControlData));
                if (controlData == NULL) {
                    break;
                }
                else {
                    memset(controlData, 0, sizeof(NsApplicationControlData));
                }

                if (R_FAILED(nsGetApplicationControlData(NsApplicationControlSource_Storage, records[i].application_id, controlData, sizeof(NsApplicationControlData), &controlSize))) continue;

                if (controlSize < sizeof(controlData->nacp)) {
                    continue;
                }

                titles.push_back(util::formatApplicationId(records[i].application_id));
            }
            free(controlData);
        }
        delete[] records;
        std::sort(titles.begin(), titles.end());
        return titles;
    }

    std::vector<std::string> excludeTitles(const std::string& path, const std::vector<std::string>& listedTitles)
    {
        std::vector<std::string> titles;
        std::ifstream file(path);
        std::string name;

        if (file.is_open()) {
            std::string line;
            while (std::getline(file, line)) {
                std::transform(line.begin(), line.end(), line.begin(), ::toupper);
                for (size_t i = 0; i < listedTitles.size(); i++) {
                    if (line == listedTitles[i]) {
                        titles.push_back(line);
                        break;
                    }
                }
            }
        }

        std::sort(titles.begin(), titles.end());

        std::vector<std::string> diff;
        std::set_difference(listedTitles.begin(), listedTitles.end(), titles.begin(), titles.end(),
                            std::inserter(diff, diff.begin()));
        return diff;
    }

    int computeOffset(CFW cfw)
    {
        switch (cfw) {
            case CFW::ams:
                std::filesystem::create_directory(AMS_PATH);
                std::filesystem::create_directory(AMS_CONTENTS);
                chdir(AMS_PATH);
                return std::string(CONTENTS_PATH).length();
                break;
            case CFW::rnx:
                std::filesystem::create_directory(REINX_PATH);
                std::filesystem::create_directory(REINX_CONTENTS);
                chdir(REINX_PATH);
                return std::string(CONTENTS_PATH).length();
                break;
            case CFW::sxos:
                std::filesystem::create_directory(SXOS_PATH);
                std::filesystem::create_directory(SXOS_TITLES);
                chdir(SXOS_PATH);
                return std::string(TITLES_PATH).length();
                break;
        }
        return 0;
    }

    void extractCheats(const std::string& archivePath, const std::vector<std::string>& titles, CFW cfw, const std::string& version, bool extractAll)
    {
        ensureAvailableStorage(archivePath);

        unzFile zfile = unzOpen(archivePath.c_str());
        unz_global_info gi;
        unzGetGlobalInfo(zfile, &gi);

        ProgressEvent::instance().setTotalSteps(gi.number_entry);
        ProgressEvent::instance().setStep(0);

        int offset = computeOffset(cfw);

        for (uLong i = 0; i < gi.number_entry; ++i) {
            char szFilename[0x301] = "";
            unzOpenCurrentFile(zfile);
            unzGetCurrentFileInfo(zfile, NULL, szFilename, sizeof(szFilename), NULL, 0, NULL, 0);
            std::string filename = szFilename;

            if (ProgressEvent::instance().getInterupt()) {
                unzCloseCurrentFile(zfile);
                break;
            }

            if ((int)filename.size() > offset + 16 + 7 && caselessCompare(filename.substr(offset + 16, 7), "/cheats")) {
                if (extractAll) {
                    extractEntry(filename, zfile);
                }
                else {
                    if (std::find_if(titles.begin(), titles.end(), [&filename, offset](std::string title) {
                            return caselessCompare((title.substr(0, 13)), filename.substr(offset, 13));
                        }) != titles.end()) {
                        extractEntry(filename, zfile);
                    }
                }
            }

            ProgressEvent::instance().setStep(i);
            unzCloseCurrentFile(zfile);
            unzGoToNextFile(zfile);
        }
        unzClose(zfile);
        if (version != "offline" && version != "") {
            util::saveToFile(version, CHEATS_VERSION);
        }
        ProgressEvent::instance().setStep(ProgressEvent::instance().getMax());
    }

    void extractAllCheats(const std::string& archivePath, CFW cfw, const std::string& version)
    {
        extractCheats(archivePath, {}, cfw, version, true);
    }

    bool isBID(const std::string& bid)
    {
        for (char const& c : bid) {
            if (!isxdigit(c)) return false;
        }
        return true;
    }

    void writeTitlesToFile(const std::set<std::string>& titles, const std::string& path)
    {
        std::ofstream updatedTitlesFile;
        std::set<std::string>::iterator it = titles.begin();
        updatedTitlesFile.open(path, std::ofstream::out | std::ofstream::trunc);
        if (updatedTitlesFile.is_open()) {
            while (it != titles.end()) {
                updatedTitlesFile << (*it) << std::endl;
                it++;
            }
            updatedTitlesFile.close();
        }
    }

    void removeCheats()
    {
        std::string path = util::getContentsPath();
        ProgressEvent::instance().setTotalSteps(std::distance(std::filesystem::directory_iterator(path), std::filesystem::directory_iterator()) + 1);
        for (const auto& entry : std::filesystem::directory_iterator(path)) {
            if (ProgressEvent::instance().getInterupt()) {
                break;
            }
            removeCheatsDirectory(entry.path().string());
            ProgressEvent::instance().incrementStep(1);
        }
        std::filesystem::remove(CHEATS_VERSION);
        ProgressEvent::instance().setStep(ProgressEvent::instance().getMax());
    }

    void removeOrphanedCheats()
    {
        auto path = util::getContentsPath();
        std::vector<std::string> titles = getInstalledTitlesNs();
        ProgressEvent::instance().setTotalSteps(std::distance(std::filesystem::directory_iterator(path), std::filesystem::directory_iterator()) + 1);
        for (const auto& entry : std::filesystem::directory_iterator(path)) {
            if (ProgressEvent::instance().getInterupt()) {
                break;
            }
            if (std::find_if(titles.begin(), titles.end(), [&entry](std::string title) {
                    return caselessCompare(entry.path().filename(), title);
                }) == titles.end()) {
                removeCheatsDirectory(entry.path().string());
            }
            ProgressEvent::instance().incrementStep(1);
        }
        std::filesystem::remove(CHEATS_VERSION);
        ProgressEvent::instance().setStep(ProgressEvent::instance().getMax());
    }

    bool removeCheatsDirectory(const std::string& entry)
    {
        bool res = true;
        std::string cheatsPath = fmt::format("{}/cheats", entry);
        if (std::filesystem::exists(cheatsPath)) res &= fs::removeDir(cheatsPath);
        if (std::filesystem::is_empty(entry)) res &= fs::removeDir(entry);
        return res;
    }

}  // namespace extract

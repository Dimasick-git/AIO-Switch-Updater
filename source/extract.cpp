#include "extract.hpp"

#include <dirent.h>
#include <minizip/unzip.h>

#include <algorithm>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iterator>
#include <limits>
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
            if (!zfile)
                return false;

            unz_global_info gi{};
            bool ok = unzGetGlobalInfo(zfile, &gi) == UNZ_OK;
            if (ok && gi.number_entry > 0)
                ok = unzGoToFirstFile(zfile) == UNZ_OK;

            for (uLong i = 0; ok && i < gi.number_entry; ++i) {
                unz_file_info fi{};
                ok = unzGetCurrentFileInfo(zfile, &fi, NULL, 0, NULL, 0, NULL, 0) == UNZ_OK;
                if (!ok)
                    break;
                if (fi.uncompressed_size > static_cast<uLong>(std::numeric_limits<s64>::max() - size)) {
                    ok = false;
                    break;
                }
                size += static_cast<s64>(fi.uncompressed_size);
                if (i + 1 < gi.number_entry)
                    ok = unzGoToNextFile(zfile) == UNZ_OK;
            }
            unzClose(zfile);
            return ok;
        }

        bool ensureAvailableStorage(const std::string& archivePath)
        {
            s64 uncompressedSize = 0;
            if (!getUncompressedSize(archivePath, uncompressedSize))
                return false;

            s64 freeStorage = 0;
            if (R_SUCCEEDED(fs::getFreeStorageSD(freeStorage))) {
                brls::Logger::info("Uncompressed size of archive {}: {}. Available: {}", archivePath, uncompressedSize, freeStorage);
                // Leave a 10% safety margin for filesystem metadata and active app files.
                if (uncompressedSize > freeStorage - (freeStorage / 10))
                    return false;
            }
            return true;
        }

        bool isUnsafeArchivePath(const std::string& entry)
        {
            if (entry.empty() || entry.front() == '/')
                return true;
            const std::filesystem::path path(entry);
            if (path.is_absolute())
                return true;
            for (const auto& component : path) {
                if (component == "..")
                    return true;
            }
            return false;
        }

        bool extractEntry(const std::string& filename, unzFile& zfile)
        {
            if (filename.empty())
                return false;

            std::error_code ec;
            if (filename.back() == '/') {
                std::filesystem::create_directories(filename, ec);
                return !ec;
            }

            const std::filesystem::path outputPath(filename);
            const auto parent = outputPath.parent_path();
            if (!parent.empty())
                std::filesystem::create_directories(parent, ec);
            if (ec)
                return false;

            FILE* outfile = fopen(filename.c_str(), "wb");
            if (!outfile)
                return false;

            std::vector<unsigned char> buf(WRITE_BUFFER_SIZE);
            bool ok = true;
            for (;;) {
                const int read = unzReadCurrentFile(zfile, buf.data(), static_cast<unsigned int>(buf.size()));
                if (read < 0) {
                    ok = false;
                    break;
                }
                if (read == 0)
                    break;
                if (fwrite(buf.data(), 1, static_cast<size_t>(read), outfile) != static_cast<size_t>(read)) {
                    ok = false;
                    break;
                }
            }
            if (fclose(outfile) != 0)
                ok = false;
            return ok;
        }
    }  // namespace

    bool extract(const std::string& archivePath, const std::string& workingPath, bool preserveInis, std::function<void()> func)
    {
        if (!ensureAvailableStorage(archivePath))
            return false;

        unzFile zfile = unzOpen(archivePath.c_str());
        if (!zfile)
            return false;

        unz_global_info gi{};
        bool ok = unzGetGlobalInfo(zfile, &gi) == UNZ_OK;
        if (ok && gi.number_entry > 0)
            ok = unzGoToFirstFile(zfile) == UNZ_OK;

        ProgressEvent::instance().setTotalSteps(gi.number_entry);
        ProgressEvent::instance().setStep(0);

        const std::set<std::string> ignoreList = fs::readLineByLine(FILES_IGNORE);
        const std::string appPath = util::getAppPath();

        for (uLong i = 0; ok && i < gi.number_entry; ++i) {
            char szFilename[0x301] = "";
            unz_file_info fileInfo{};
            ok = unzGetCurrentFileInfo(zfile, &fileInfo, szFilename, sizeof(szFilename), NULL, 0, NULL, 0) == UNZ_OK;
            if (!ok || fileInfo.size_filename >= sizeof(szFilename)) {
                ok = false;
                break;
            }

            const std::string entryName(szFilename);
            if (isUnsafeArchivePath(entryName)) {
                ok = false;
                break;
            }
            const std::string filename = workingPath + entryName;
            if (ProgressEvent::instance().getInterupt()) {
                ok = false;
                break;
            }

            ok = unzOpenCurrentFile(zfile) == UNZ_OK;
            if (!ok)
                break;

            bool extracted = true;
            if (appPath != filename) {
                const bool isIni = filename.size() >= 4 && filename.compare(filename.size() - 4, 4, ".ini") == 0;
                const bool ignored = std::any_of(ignoreList.begin(), ignoreList.end(), [&filename](const std::string& ignoredPath) {
                    const std::size_t pos = filename.find(ignoredPath);
                    return pos == 0 || pos == 1;
                });
                if ((preserveInis && isIni) || ignored) {
                    if (!std::filesystem::exists(filename))
                        extracted = extractEntry(filename, zfile);
                }
                else if ((filename == "/atmosphere/package3") || (filename == "/atmosphere/stratosphere.romfs")) {
                    extracted = extractEntry(filename + ".aio", zfile);
                }
                else {
                    extracted = extractEntry(filename, zfile);
                    if (extracted && filename.rfind("/hekate_ctcaer", 0) == 0) {
                        if (!fs::copyFile(filename, UPDATE_BIN_PATH))
                            extracted = false;
                        else if (CurrentCfw::running_cfw == CFW::ams && util::showDialogBoxBlocking(fmt::format("menus/utils/set_hekate_reboot_payload"_i18n, UPDATE_BIN_PATH, REBOOT_PAYLOAD_PATH), "menus/common/yes"_i18n, "menus/common/no"_i18n) == 0) {
                            extracted = fs::copyFile(UPDATE_BIN_PATH, REBOOT_PAYLOAD_PATH);
                        }
                    }
                }
            }

            const int closeResult = unzCloseCurrentFile(zfile);
            if (!extracted || closeResult != UNZ_OK) {
                ok = false;
                break;
            }
            ProgressEvent::instance().setStep(i + 1);
            if (i + 1 < gi.number_entry)
                ok = unzGoToNextFile(zfile) == UNZ_OK;
        }
        unzClose(zfile);
        if (ok)
            func();
        ProgressEvent::instance().setStep(ProgressEvent::instance().getMax());
        return ok;
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

/**
 * @file ryazhenka_backup.cpp
 * @brief See ryazhenka_backup.hpp.
 * @author Dimasick-git
 */

#include "ryazhenka_backup.hpp"

#include <chrono>
#include <cstdio>
#include <ctime>
#include <filesystem>
#include <fstream>
#include <string>
#include <system_error>

#include "ryazhenka_logger.hpp"

namespace ryazhenka::backup {

namespace {

constexpr const char kAtmospherePath[] = "/atmosphere";
constexpr const char kBackupRoot[] = "/aio-backup";
constexpr const char kConfigFile[] = "/config/aio-switch-updater/config.json";
constexpr const char kAutoBackupKey[] = "auto_backup_before_pack";

std::string utcStamp() {
    using namespace std::chrono;
    const auto now = system_clock::now();
    const auto t = system_clock::to_time_t(now);
    std::tm tm{};
    gmtime_r(&t, &tm);
    char buf[32];
    std::snprintf(buf, sizeof buf, "%04d%02d%02d-%02d%02d",
                  tm.tm_year + 1900, tm.tm_mon + 1, tm.tm_mday,
                  tm.tm_hour, tm.tm_min);
    return buf;
}

} // namespace

std::string snapshotAtmosphere() {
    std::error_code ec;
    if (!std::filesystem::is_directory(kAtmospherePath, ec)) {
        ryazhenka::log::warn("backup: /atmosphere is not a directory; skipping");
        return {};
    }

    const std::string dest =
        std::string(kBackupRoot) + "/atmosphere-" + utcStamp();

    std::filesystem::create_directories(dest, ec);
    if (ec) {
        ryazhenka::log::error("backup: cannot mkdir " + dest + ": " + ec.message());
        return {};
    }

    const auto opts = std::filesystem::copy_options::recursive |
                      std::filesystem::copy_options::skip_existing |
                      std::filesystem::copy_options::copy_symlinks;
    std::filesystem::copy(kAtmospherePath, dest, opts, ec);
    if (ec) {
        ryazhenka::log::warn("backup: completed with errors: " + ec.message());
    } else {
        ryazhenka::log::info("backup: snapshot written to " + dest);
    }
    return dest;
}

bool isAutoBackupEnabled() {
    std::ifstream f(kConfigFile);
    if (!f.is_open()) return true; // default ON if config not present yet
    std::string body((std::istreambuf_iterator<char>(f)),
                     std::istreambuf_iterator<char>());
    const auto key_pos = body.find(kAutoBackupKey);
    if (key_pos == std::string::npos) return true;
    // Cheap parser: look at the first true/false token after the key.
    const auto true_pos  = body.find("true",  key_pos);
    const auto false_pos = body.find("false", key_pos);
    if (true_pos == std::string::npos) return false;
    if (false_pos != std::string::npos && false_pos < true_pos) return false;
    return true;
}

std::size_t pruneOlderThan(int days) {
    std::error_code ec;
    if (!std::filesystem::is_directory(kBackupRoot, ec)) return 0;

    const auto threshold = std::chrono::system_clock::now() -
                           std::chrono::hours(24 * days);

    std::size_t removed = 0;
    for (const auto& entry : std::filesystem::directory_iterator(kBackupRoot, ec)) {
        if (ec) break;
        const auto wt = std::filesystem::last_write_time(entry, ec);
        if (ec) { ec.clear(); continue; }
        // file_time → system_clock conversion: assume same epoch on libnx.
        const auto sct = std::chrono::time_point_cast<std::chrono::system_clock::duration>(
            wt - std::filesystem::file_time_type::clock::now() +
            std::chrono::system_clock::now());
        if (sct < threshold) {
            std::filesystem::remove_all(entry.path(), ec);
            if (!ec) {
                ryazhenka::log::info("backup: pruned " + entry.path().string());
                ++removed;
            }
            ec.clear();
        }
    }
    return removed;
}

} // namespace ryazhenka::backup

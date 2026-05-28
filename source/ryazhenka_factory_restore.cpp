/**
 * @file ryazhenka_factory_restore.cpp
 * @brief See ryazhenka_factory_restore.hpp.
 * @author Dimasick-git
 */

#include "ryazhenka_factory_restore.hpp"

#include <algorithm>
#include <cstdio>
#include <string_view>
#include <system_error>

#include "ryazhenka_backup.hpp"
#include "ryazhenka_logger.hpp"

namespace fs = std::filesystem;

namespace ryazhenka::restore {

namespace {

constexpr const char* kBackupRoot       = "/aio-backup";
constexpr const char* kAtmospherePath   = "/atmosphere";
constexpr std::string_view kEntryPrefix = "atmosphere-";

void measure(const fs::path& dir, std::uint64_t& bytes, std::size_t& files) {
    std::error_code ec;
    if (!fs::is_directory(dir, ec)) return;
    for (auto it = fs::recursive_directory_iterator(dir, ec);
         it != fs::recursive_directory_iterator{} && !ec;
         it.increment(ec)) {
        if (!it->is_regular_file()) continue;
        const auto sz = fs::file_size(it->path(), ec);
        if (ec) { ec.clear(); continue; }
        bytes += sz;
        ++files;
    }
}

} // anonymous

std::vector<BackupEntry> listBackups(std::size_t limit) {
    std::vector<BackupEntry> out;
    std::error_code ec;

    if (!fs::is_directory(kBackupRoot, ec)) {
        log::info("restore: /aio-backup missing — no snapshots");
        return out;
    }

    for (auto it = fs::directory_iterator(kBackupRoot, ec);
         it != fs::directory_iterator{} && !ec; it.increment(ec)) {
        if (!it->is_directory()) continue;
        const auto name = it->path().filename().string();
        if (name.find(kEntryPrefix) != 0) continue;
        BackupEntry e;
        e.path  = it->path();
        e.mtime = fs::last_write_time(e.path, ec);
        if (ec) { ec.clear(); }
        out.push_back(std::move(e));
    }

    // Newest first.
    std::sort(out.begin(), out.end(), [](const BackupEntry& a, const BackupEntry& b) {
        return a.mtime > b.mtime;
    });

    if (limit > 0 && out.size() > limit) out.resize(limit);

    // Now compute sizes (only for the entries we will actually display).
    for (auto& e : out) {
        measure(e.path, e.size_bytes, e.file_count);
    }

    log::info("restore: found " + std::to_string(out.size()) + " snapshots");
    return out;
}

bool restoreFrom(const BackupEntry& entry, ProgressFn progress) {
    std::error_code ec;
    if (!fs::is_directory(entry.path, ec)) {
        log::warn("restore: backup path missing " + entry.path.string());
        return false;
    }

    // 1) Defensive snapshot of the *current* /atmosphere/ — if the user
    //    realises mid-restore that they picked the wrong snapshot, the
    //    state right before the restore is still on disk.
    if (progress) progress(Stage::pre_snapshot, 0, 0);
    const std::string preSnap = backup::snapshotAtmosphere();
    if (preSnap.empty()) {
        log::warn("restore: pre-snapshot failed — proceeding anyway is risky, aborting");
        return false;
    }
    log::info("restore: pre-snapshot at " + preSnap);

    // 2) Wipe the live tree. We do this rather than overlay-copy because
    //    a partial restore would leave stale files from the previous
    //    install lying around, and those are the worst kind of bug.
    if (progress) progress(Stage::wiping, 0, 0);
    fs::remove_all(kAtmospherePath, ec);
    if (ec) {
        log::warn("restore: wipe /atmosphere failed — " + ec.message());
        return false;
    }
    fs::create_directories(kAtmospherePath, ec);
    if (ec) {
        log::warn("restore: mkdir /atmosphere failed — " + ec.message());
        return false;
    }

    // 3) Stream the snapshot back in. Walked manually so we can drive
    //    the progress callback; std::filesystem::copy(recursive) is a
    //    blackbox.
    if (progress) progress(Stage::copying, 0, entry.file_count);
    std::size_t copied = 0;
    for (auto it = fs::recursive_directory_iterator(entry.path, ec);
         it != fs::recursive_directory_iterator{} && !ec;
         it.increment(ec)) {
        const auto rel = it->path().lexically_relative(entry.path);
        const fs::path dst = fs::path(kAtmospherePath) / rel;
        std::error_code one_ec;
        if (it->is_directory()) {
            fs::create_directories(dst, one_ec);
        } else if (it->is_regular_file()) {
            fs::create_directories(dst.parent_path(), one_ec);
            fs::copy_file(it->path(), dst,
                          fs::copy_options::overwrite_existing, one_ec);
            ++copied;
            if (progress && (copied % 32 == 0)) {
                progress(Stage::copying, copied, entry.file_count);
            }
        } else if (it->is_symlink()) {
            fs::copy_symlink(it->path(), dst, one_ec);
        }
        if (one_ec) {
            log::warn("restore: copy " + rel.string() + " — " + one_ec.message());
        }
    }

    if (progress) progress(Stage::done, copied, entry.file_count);
    log::info("restore: " + std::to_string(copied) + " files restored from " +
              entry.path.string());
    return true;
}

std::string formatLabel(const BackupEntry& entry) {
    const std::string name = entry.path.filename().string();
    // Expect atmosphere-YYYYMMDD-HHMM[SS], e.g. atmosphere-20260528-1430.
    if (name.size() >= std::string(kEntryPrefix).size() + 13) {
        const std::string stamp = name.substr(std::string(kEntryPrefix).size());
        char buf[64];
        if (std::sscanf(stamp.c_str(), "%*4d%*2d%*2d-%*2d%*2d") >= 0 &&
            stamp.size() >= 13) {
            // Best-effort pretty print without strptime.
            std::snprintf(buf, sizeof(buf), "%s.%s.%s %s:%s",
                          stamp.substr(6, 2).c_str(),   // DD
                          stamp.substr(4, 2).c_str(),   // MM
                          stamp.substr(0, 4).c_str(),   // YYYY
                          stamp.substr(9, 2).c_str(),   // HH
                          stamp.substr(11, 2).c_str()); // MM
            return std::string(buf);
        }
    }
    return name;
}

} // namespace ryazhenka::restore

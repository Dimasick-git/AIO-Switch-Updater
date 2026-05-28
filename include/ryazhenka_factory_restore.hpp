#pragma once

/**
 * @file ryazhenka_factory_restore.hpp
 * @brief Restore /atmosphere/ from an /aio-backup/ snapshot.
 *
 * Lists the snapshots created by ryazhenka_backup::snapshotAtmosphere(),
 * lets the caller pick one and replays it back into /atmosphere/. The
 * current /atmosphere is itself snapshotted first (anti-foot-shoot),
 * so a botched restore is still recoverable.
 *
 * @author Dimasick-git
 */

#include <chrono>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <functional>
#include <string>
#include <vector>

namespace ryazhenka::restore {

struct BackupEntry {
    std::filesystem::path path;       // /aio-backup/atmosphere-<UTC>
    std::filesystem::file_time_type mtime{};
    std::uint64_t size_bytes = 0;
    std::size_t   file_count = 0;
};

/// Lists every /aio-backup/atmosphere-* directory. Sorted by mtime DESC
/// (newest first). size_bytes + file_count are computed lazily, may be
/// expensive on huge snapshots; caller can cap by passing limit > 0.
std::vector<BackupEntry> listBackups(std::size_t limit = 0);

enum class Stage {
    pre_snapshot,
    wiping,
    copying,
    done,
};

using ProgressFn = std::function<void(Stage, std::size_t copied, std::size_t total)>;

/// Replay `entry` back into /atmosphere/. Returns true on success.
bool restoreFrom(const BackupEntry& entry, ProgressFn progress = nullptr);

/// Human-readable date label parsed from the directory name
/// (atmosphere-YYYYMMDD-HHMM) — falls back to mtime if the name does
/// not match.
std::string formatLabel(const BackupEntry& entry);

} // namespace ryazhenka::restore

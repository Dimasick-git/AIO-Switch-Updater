#pragma once

/**
 * @file ryazhenka_backup.hpp
 * @brief Snapshot critical CFW directories before destructive operations.
 * @author Dimasick-git
 */

#include <string>

namespace ryazhenka::backup {

/// Recursive copy of /atmosphere/ into /aio-backup/atmosphere-<UTC>/. Returns
/// the destination path on success, empty string on failure. Best-effort:
/// individual file errors are skipped, not aborted, so a partial backup is
/// better than none.
std::string snapshotAtmosphere();

/// True when /config/aio-switch-updater/config.json has "auto_backup_before_pack"
/// set to true (default = true if key absent).
bool isAutoBackupEnabled();

/// Remove /aio-backup/* entries older than `days` days. Returns number of
/// entries deleted.
std::size_t pruneOlderThan(int days);

} // namespace ryazhenka::backup

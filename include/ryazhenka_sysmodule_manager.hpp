#pragma once

/**
 * @file ryazhenka_sysmodule_manager.hpp
 * @brief Enumerate and toggle Atmosphere sysmodules via boot2.flag.
 *
 * Atmosphere's convention: a directory under /atmosphere/contents/<tid>/
 * with `flags/boot2.flag` is auto-started on boot. Removing the flag
 * disables the sysmodule (without deleting the .kip itself), creating
 * it re-enables. Title display name is best-effort: read
 * `toolbox.json` { "name": ... } when present, otherwise fall back to
 * the title id.
 *
 * All operations are pure filesystem; no IPC, no reboot required for
 * the change to take effect at next boot.
 *
 * @author Dimasick-git
 */

#include <optional>
#include <string>
#include <vector>

namespace ryazhenka::sysmodule {

struct Entry {
    std::string title_id;             // hex, lowercased
    std::string display_name;         // human-readable, falls back to title_id
    bool        enabled = false;      // boot2.flag exists
    bool        critical = false;     // fs / loader / sm — never toggle
    std::string dir;                  // /atmosphere/contents/<tid>
};

/// Lists every directory under /atmosphere/contents/ that looks like a
/// sysmodule (title id pattern + non-empty content). Sorted by display name.
std::vector<Entry> list();

/// Create or remove `<dir>/flags/boot2.flag`. Returns false on IO error.
bool setEnabled(const Entry& entry, bool enable);

/// Title IDs we refuse to toggle (system-critical builtins).
bool isCriticalTitleId(const std::string& tid);

} // namespace ryazhenka::sysmodule

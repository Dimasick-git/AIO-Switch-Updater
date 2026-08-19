#pragma once

// Version checker for the Ryazhenka ecosystem (applications, overlays,
// sysmodules and SD packages). Reads romfs:/ryazhenka_tools.json, compares the
// NACP display_version for .nro applications, and checks file presence for
// overlays, sysmodules and packages. Every component also receives the latest
// GitHub release tag. The check is fire-and-forget — failures (no internet,
// missing file, malformed NACP, GitHub rate-limit) are downgraded to a log line.

#include <optional>
#include <string>
#include <vector>

namespace ryazhenka::version_check {

struct ToolStatus {
    std::string name;
    std::string repo;
    std::string local_path;
    std::optional<std::string> local_version;
    std::optional<std::string> latest_version;
    bool update_available = false;
    std::string note;
};

// Read romfs:/ryazhenka_tools.json and return one ToolStatus per declared
// component (tools + overlays + sysmodules + packages categories merged).
// Network calls happen here; expect this to take up to
// (Nentries * kCurlConnectTimeoutSec) seconds in the worst case. Run from a
// worker thread.
std::vector<ToolStatus> runChecks();

// Try to read the NACP display_version field from an .nro file on the SD card.
// Returns nullopt if the file does not exist or does not look like a valid
// NRO+ASET+NACP layout.
std::optional<std::string> readNroDisplayVersion(const std::string& nroPath);

// Fire the version check on a detached thread. Safe to call multiple times —
// only the first call actually does work.
void scheduleBackgroundCheck();

} // namespace ryazhenka::version_check

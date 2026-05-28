#pragma once

// Version checker for the Ryazhenka ecosystem (Dimasick-git/* tools, overlays
// and sysmodules). Reads the list from romfs:/ryazhenka_tools.json, parses the
// NACP display_version from each locally installed .nro, fetches the latest
// tag from the corresponding GitHub releases endpoint, and reports which tools
// need updating. The check is fire-and-forget — failures (no internet, missing
// file, malformed NACP, GitHub rate-limit) are downgraded to a log line.

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

// Read romfs:/ryazhenka_tools.json and return one ToolStatus per declared tool
// (tools + overlays + sysmodules categories merged). Network calls happen
// here; expect this to take up to (Nentries * kCurlConnectTimeoutSec) seconds
// in the worst case. Run from a worker thread.
std::vector<ToolStatus> runChecks();

// Try to read the NACP display_version field from an .nro file on the SD card.
// Returns nullopt if the file does not exist or does not look like a valid
// NRO+ASET+NACP layout.
std::optional<std::string> readNroDisplayVersion(const std::string& nroPath);

// Fire the version check on a detached thread. Safe to call multiple times —
// only the first call actually does work.
void scheduleBackgroundCheck();

} // namespace ryazhenka::version_check

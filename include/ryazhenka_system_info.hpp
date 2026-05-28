#pragma once

// Lightweight system-state collector. All fields are best-effort — missing
// values default to empty/0 instead of throwing, so callers can format the
// struct unconditionally.

#include <cstdint>
#include <string>

namespace ryazhenka::sysinfo {

struct Snapshot {
    std::string ams_version;       // e.g. "1.10.0" if /atmosphere/release_ver exists
    std::string cfw_kind;          // "Atmosphere" / "ReiNX" / "SX OS" / "?"
    std::uint64_t sd_free_bytes = 0;
    std::uint64_t sd_total_bytes = 0;
    std::size_t ams_contents_count = 0;   // entries under /atmosphere/contents/
    std::size_t exefs_patches_count = 0;  // entries under /atmosphere/exefs_patches/
    bool has_sigpatches = false;
    bool has_hekate = false;
    bool has_ryazhenka_marker = false;    // /switch/.ryazhenka marker file
};

Snapshot collect();

// Single-line human summary, useful for the SD log on startup.
std::string formatOneLine(const Snapshot& s);

// Quick free-space probe without taking a full snapshot. Returns true if the
// SD card has at least `min_bytes` of free space available. False on any
// filesystem error (better to be conservative and warn the user).
bool hasEnoughFreeSpace(std::uint64_t min_bytes);

} // namespace ryazhenka::sysinfo

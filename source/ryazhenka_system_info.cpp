#include "ryazhenka_system_info.hpp"

#include <cstdio>
#include <filesystem>
#include <fstream>
#include <string>
#include <system_error>

namespace ryazhenka::sysinfo {

namespace {

std::string readFirstLine(const std::filesystem::path& path) {
    std::ifstream f(path);
    if (!f.is_open()) return {};
    std::string line;
    std::getline(f, line);
    while (!line.empty() && (line.back() == '\r' || line.back() == '\n' || line.back() == ' ')) {
        line.pop_back();
    }
    return line;
}

std::size_t countEntries(const std::filesystem::path& dir) {
    std::error_code ec;
    if (!std::filesystem::is_directory(dir, ec)) return 0;
    std::size_t n = 0;
    for (auto it = std::filesystem::directory_iterator(dir, ec);
         it != std::filesystem::directory_iterator{} && !ec; it.increment(ec)) {
        (void)*it;
        ++n;
    }
    return n;
}

std::string humanBytes(std::uint64_t bytes) {
    constexpr double kKi = 1024.0;
    constexpr const char* units[] = {"B", "KiB", "MiB", "GiB", "TiB"};
    double v = static_cast<double>(bytes);
    int unit = 0;
    while (v >= kKi && unit < 4) { v /= kKi; ++unit; }
    char buf[32];
    std::snprintf(buf, sizeof buf, "%.1f %s", v, units[unit]);
    return buf;
}

} // namespace

Snapshot collect() {
    Snapshot s;
    std::error_code ec;

    s.ams_version = readFirstLine("/atmosphere/release_ver");

    if (std::filesystem::is_directory("/atmosphere", ec)) {
        s.cfw_kind = "Atmosphere";
    } else if (std::filesystem::is_directory("/ReiNX", ec)) {
        s.cfw_kind = "ReiNX";
    } else if (std::filesystem::is_directory("/sxos", ec)) {
        s.cfw_kind = "SX OS";
    } else {
        s.cfw_kind = "?";
    }

    if (const auto space = std::filesystem::space("/", ec); !ec) {
        s.sd_free_bytes = space.available;
        s.sd_total_bytes = space.capacity;
    }

    s.ams_contents_count = countEntries("/atmosphere/contents");
    s.exefs_patches_count = countEntries("/atmosphere/exefs_patches");
    s.has_sigpatches = s.exefs_patches_count > 0;
    s.has_hekate = std::filesystem::exists("/bootloader/hekate_ipl.ini", ec);
    s.has_ryazhenka_marker = std::filesystem::exists("/switch/.ryazhenka", ec) ||
                              std::filesystem::exists("/config/ryazhahand", ec);

    return s;
}

std::string formatOneLine(const Snapshot& s) {
    std::string line;
    line.reserve(256);
    line += "cfw=" + s.cfw_kind;
    if (!s.ams_version.empty()) line += " ams=" + s.ams_version;
    line += " sigpatches=" + std::string(s.has_sigpatches ? "yes" : "no");
    line += " hekate=" + std::string(s.has_hekate ? "yes" : "no");
    line += " ryazhenka=" + std::string(s.has_ryazhenka_marker ? "yes" : "no");
    line += " contents=" + std::to_string(s.ams_contents_count);
    line += " patches=" + std::to_string(s.exefs_patches_count);
    if (s.sd_total_bytes > 0) {
        line += " sd=" + humanBytes(s.sd_free_bytes) + "/" + humanBytes(s.sd_total_bytes);
    }
    return line;
}

bool hasEnoughFreeSpace(std::uint64_t min_bytes) {
    std::error_code ec;
    const auto space = std::filesystem::space("/", ec);
    if (ec) return false;
    return space.available >= min_bytes;
}

} // namespace ryazhenka::sysinfo

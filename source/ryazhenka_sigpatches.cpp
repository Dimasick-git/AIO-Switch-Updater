/**
 * @file ryazhenka_sigpatches.cpp
 * @brief See ryazhenka_sigpatches.hpp.
 * @author Dimasick-git
 */

#include "ryazhenka_sigpatches.hpp"

#include <algorithm>
#include <chrono>
#include <cstdio>
#include <ctime>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>
#include <system_error>
#include <vector>

#include <json.hpp>

#include "download.hpp"
#include "ryazhenka_config.hpp"
#include "ryazhenka_logger.hpp"
#include "ryazhenka_sha256.hpp"

namespace fs = std::filesystem;

namespace ryazhenka::sigpatches {

namespace {

constexpr const char* kPatchesDir = "/atmosphere/exefs_patches";

using SysTime = std::chrono::system_clock::time_point;

std::optional<SysTime> parseIso8601(const std::string& s) {
    // Accepts "2026-05-28T14:30:00Z" — GitHub published_at format.
    if (s.size() < 20) return std::nullopt;
    std::tm tm{};
#if defined(__GLIBCXX__) || defined(__GNUC__)
    if (!strptime(s.c_str(), "%Y-%m-%dT%H:%M:%SZ", &tm)) return std::nullopt;
#else
    int yr, mo, da, hr, mi, se;
    if (std::sscanf(s.c_str(), "%d-%d-%dT%d:%d:%dZ",
                    &yr, &mo, &da, &hr, &mi, &se) != 6) return std::nullopt;
    tm.tm_year = yr - 1900;
    tm.tm_mon  = mo - 1;
    tm.tm_mday = da;
    tm.tm_hour = hr;
    tm.tm_min  = mi;
    tm.tm_sec  = se;
#endif
    const std::time_t tt = timegm(&tm);
    if (tt == static_cast<std::time_t>(-1)) return std::nullopt;
    return std::chrono::system_clock::from_time_t(tt);
}

int daysBetween(SysTime a, SysTime b) {
    const auto diff = std::chrono::duration_cast<std::chrono::hours>(b - a).count();
    return static_cast<int>(diff / 24);
}

} // anonymous

std::string localFingerprint() {
    std::error_code ec;
    if (!fs::is_directory(kPatchesDir, ec)) return {};

    std::vector<std::string> rows;
    for (auto it = fs::recursive_directory_iterator(kPatchesDir, ec);
         it != fs::recursive_directory_iterator{} && !ec;
         it.increment(ec)) {
        if (!it->is_regular_file()) continue;
        const auto sz = fs::file_size(it->path(), ec);
        const auto mt = fs::last_write_time(it->path(), ec);
        if (ec) { ec.clear(); continue; }
        std::ostringstream row;
        row << it->path().lexically_relative(kPatchesDir).string()
            << '|' << sz
            << '|' << mt.time_since_epoch().count();
        rows.push_back(row.str());
    }
    std::sort(rows.begin(), rows.end());

    // Stream into mbedtls via a temp file is overkill; we have the raw
    // bytes in-memory. Build a single string and SHA-256 over it.
    std::string concat;
    for (const auto& r : rows) { concat.append(r); concat.push_back('\n'); }

    // Use mbedtls directly here — sha256::ofFile is file-based.
    return sha256::ofBuffer(concat);
}

std::optional<int> daysSinceLocalUpdate() {
    std::error_code ec;
    if (!fs::is_directory(kPatchesDir, ec)) return std::nullopt;

    fs::file_time_type newest = fs::file_time_type::min();
    bool any = false;
    for (auto it = fs::recursive_directory_iterator(kPatchesDir, ec);
         it != fs::recursive_directory_iterator{} && !ec;
         it.increment(ec)) {
        if (!it->is_regular_file()) continue;
        const auto mt = fs::last_write_time(it->path(), ec);
        if (ec) { ec.clear(); continue; }
        if (mt > newest) newest = mt;
        any = true;
    }
    if (!any) return std::nullopt;

    const auto sys_now = std::chrono::system_clock::now();
    // file_time_type is not always sys_clock; convert defensively via
    // file_clock::now() delta. On libstdc++ aarch64-none-elf they are
    // typically aliased — we compute the gap from "newest" to "file now"
    // and treat that as the gap from "now".
    const auto file_now = fs::file_time_type::clock::now();
    const auto delta    = file_now - newest;
    const auto delta_h  = std::chrono::duration_cast<std::chrono::hours>(delta).count();
    (void)sys_now;
    return static_cast<int>(delta_h / 24);
}

std::optional<int> daysSinceRemoteRelease() {
    nlohmann::ordered_json cached;
    std::error_code ec;

    // Honour cache.
    const fs::path cachePath = kSigpatchesRemoteCachePath;
    bool cache_fresh = false;
    if (fs::exists(cachePath, ec)) {
        const auto mt = fs::last_write_time(cachePath, ec);
        if (!ec) {
            const auto age_h = std::chrono::duration_cast<std::chrono::hours>(
                fs::file_time_type::clock::now() - mt).count();
            if (age_h < kSigpatchesCacheTtlHours) cache_fresh = true;
        }
    }

    nlohmann::ordered_json payload;
    if (cache_fresh) {
        try {
            std::ifstream f(cachePath);
            f >> payload;
        } catch (...) { cache_fresh = false; }
    }

    if (!cache_fresh) {
        long http = download::getRequest(kSigpatchesReleasesUrl, payload);
        if (http != 200) {
            log::warn("sigpatches: remote check http=" + std::to_string(http));
            return std::nullopt;
        }
        try {
            std::ofstream o(cachePath);
            o << payload.dump();
        } catch (...) { /* cache best-effort */ }
    }

    if (!payload.contains("published_at") || !payload["published_at"].is_string()) {
        return std::nullopt;
    }
    const auto when = parseIso8601(payload["published_at"].get<std::string>());
    if (!when) return std::nullopt;
    return daysBetween(*when, std::chrono::system_clock::now());
}

health::Issue evaluate() {
    health::Issue issue;
    issue.title = "sigpatches актуальность";

    const auto local = daysSinceLocalUpdate();
    if (!local) {
        issue.severity = health::Severity::warn;
        issue.detail   = "exefs_patches пуст или недоступен";
        return issue;
    }

    const auto remote = daysSinceRemoteRelease();
    if (!remote) {
        issue.severity = health::Severity::ok;
        issue.detail   = "локально обновлены " + std::to_string(*local) +
                         " дн. назад (удалённый источник недоступен)";
        return issue;
    }

    // If remote is much newer than local — staleness.
    const int gap = *local - *remote;
    if (gap >= kSigpatchesStaleThresholdDays) {
        issue.severity = health::Severity::warn;
        issue.detail   = "локальная копия на " + std::to_string(gap) +
                         " дн. старше последнего релиза";
    } else {
        issue.severity = health::Severity::ok;
        issue.detail   = "локально " + std::to_string(*local) +
                         " дн., релиз " + std::to_string(*remote) + " дн.";
    }
    return issue;
}

} // namespace ryazhenka::sigpatches

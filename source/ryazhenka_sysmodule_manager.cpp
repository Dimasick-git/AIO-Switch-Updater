/**
 * @file ryazhenka_sysmodule_manager.cpp
 * @brief See ryazhenka_sysmodule_manager.hpp.
 * @author Dimasick-git
 */

#include "ryazhenka_sysmodule_manager.hpp"

#include <algorithm>
#include <array>
#include <cctype>
#include <filesystem>
#include <fstream>
#include <string>
#include <system_error>

#include <json.hpp>

#include "ryazhenka_logger.hpp"

namespace fs = std::filesystem;

namespace ryazhenka::sysmodule {

namespace {

constexpr const char* kContentsRoot = "/atmosphere/contents";

// Atmosphere builtins / minimum Switch services. Refuse to toggle these —
// disabling fs/loader/sm bricks the boot to recovery.
constexpr std::array<const char*, 7> kCriticalIds = {
    "0100000000000000",  // fs (system)
    "0100000000000010",  // sm
    "0100000000000034",  // loader
    "0100000000000037",  // ncm
    "0100000000000036",  // pm
    "010000000000000d",  // boot
    "010000000000004d",  // ams_mitm
};

bool looksLikeTitleId(std::string_view s) {
    if (s.size() != 16) return false;
    for (char c : s) {
        if (!std::isxdigit(static_cast<unsigned char>(c))) return false;
    }
    return true;
}

std::string toLower(std::string s) {
    for (auto& c : s) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    return s;
}

std::string readToolboxName(const fs::path& dir) {
    const auto tb = dir / "toolbox.json";
    std::error_code ec;
    if (!fs::exists(tb, ec)) return {};
    try {
        std::ifstream f(tb);
        if (!f.is_open()) return {};
        nlohmann::json j;
        f >> j;
        if (j.is_object() && j.contains("name") && j["name"].is_string()) {
            return j["name"].get<std::string>();
        }
    } catch (const std::exception& e) {
        log::warn(std::string("sysmodule: toolbox.json parse failed: ") + e.what());
    } catch (...) {
        // ignore
    }
    return {};
}

} // anonymous

bool isCriticalTitleId(const std::string& tid) {
    const std::string lo = toLower(tid);
    return std::any_of(kCriticalIds.begin(), kCriticalIds.end(),
                       [&](const char* c) { return lo == c; });
}

std::vector<Entry> list() {
    std::vector<Entry> out;
    std::error_code ec;

    if (!fs::is_directory(kContentsRoot, ec)) {
        log::info("sysmodule: /atmosphere/contents missing");
        return out;
    }

    for (auto it = fs::directory_iterator(kContentsRoot, ec);
         it != fs::directory_iterator{} && !ec;
         it.increment(ec)) {
        if (!it->is_directory()) continue;
        const auto& dir = it->path();
        const std::string tid = toLower(dir.filename().string());
        if (!looksLikeTitleId(tid)) continue;

        Entry e;
        e.title_id = tid;
        e.dir      = dir.string();
        e.enabled  = fs::exists(dir / "flags" / "boot2.flag", ec);
        e.critical = isCriticalTitleId(tid);
        const std::string name = readToolboxName(dir);
        e.display_name = name.empty() ? tid : name;
        out.push_back(std::move(e));
    }

    std::sort(out.begin(), out.end(), [](const Entry& a, const Entry& b) {
        return a.display_name < b.display_name;
    });

    log::info("sysmodule: listed " + std::to_string(out.size()) + " modules");
    return out;
}

bool setEnabled(const Entry& entry, bool enable) {
    if (entry.critical) {
        log::warn("sysmodule: refusing to toggle critical tid " + entry.title_id);
        return false;
    }
    std::error_code ec;
    const fs::path flagDir = fs::path(entry.dir) / "flags";
    const fs::path flag    = flagDir / "boot2.flag";

    if (enable) {
        fs::create_directories(flagDir, ec);
        if (ec) {
            log::warn("sysmodule: mkdir " + flagDir.string() + " — " + ec.message());
            return false;
        }
        std::ofstream f(flag);   // create empty file
        if (!f.is_open()) {
            log::warn("sysmodule: cannot create " + flag.string());
            return false;
        }
    } else {
        if (fs::exists(flag, ec)) {
            fs::remove(flag, ec);
            if (ec) {
                log::warn("sysmodule: remove " + flag.string() + " — " + ec.message());
                return false;
            }
        }
    }
    log::info("sysmodule: " + entry.title_id + " → " +
              (enable ? "enabled" : "disabled"));
    return true;
}

} // namespace ryazhenka::sysmodule

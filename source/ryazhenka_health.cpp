/**
 * @file ryazhenka_health.cpp
 * @brief See ryazhenka_health.hpp.
 * @author Dimasick-git
 */

#include "ryazhenka_health.hpp"

#include <cstddef>
#include <filesystem>
#include <fstream>
#include <string>
#include <system_error>

namespace ryazhenka::health {

namespace {

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

bool fileContains(const std::filesystem::path& path, std::string_view needle) {
    std::ifstream f(path);
    if (!f.is_open()) return false;
    std::string body((std::istreambuf_iterator<char>(f)),
                     std::istreambuf_iterator<char>());
    return body.find(needle) != std::string::npos;
}

} // namespace

std::vector<Issue> run() {
    std::vector<Issue> out;
    std::error_code ec;

    // /atmosphere root present?
    if (!std::filesystem::is_directory("/atmosphere", ec)) {
        out.push_back({Severity::error, "Atmosphere", "/atmosphere directory missing"});
        return out;
    }
    out.push_back({Severity::ok, "Atmosphere", "/atmosphere present"});

    // release_ver readable?
    {
        std::ifstream f("/atmosphere/release_ver");
        if (f.is_open()) {
            std::string v;
            std::getline(f, v);
            while (!v.empty() && (v.back() == '\r' || v.back() == '\n')) v.pop_back();
            if (v.empty()) {
                out.push_back({Severity::warn, "release_ver", "exists but empty"});
            } else {
                out.push_back({Severity::ok, "release_ver", v});
            }
        } else {
            out.push_back({Severity::warn, "release_ver", "/atmosphere/release_ver missing"});
        }
    }

    // sigpatches
    {
        const auto n = countEntries("/atmosphere/exefs_patches");
        if (n == 0) {
            out.push_back({Severity::warn, "sigpatches",
                           "/atmosphere/exefs_patches пуст — установите sigpatches"});
        } else {
            out.push_back({Severity::ok, "sigpatches", std::to_string(n) + " патчей"});
        }
    }

    // contents
    {
        const auto n = countEntries("/atmosphere/contents");
        if (n == 0) {
            out.push_back({Severity::warn, "contents",
                           "/atmosphere/contents пуст — нет sysmodules/моды"});
        } else {
            out.push_back({Severity::ok, "contents", std::to_string(n) + " пакетов"});
        }
    }

    // hekate
    if (std::filesystem::exists("/bootloader/hekate_ipl.ini", ec)) {
        if (fileContains("/bootloader/hekate_ipl.ini", "[config]")) {
            out.push_back({Severity::ok, "hekate", "hekate_ipl.ini в порядке"});
        } else {
            out.push_back({Severity::warn, "hekate", "hekate_ipl.ini без секции [config]"});
        }
    } else {
        out.push_back({Severity::warn, "hekate", "/bootloader/hekate_ipl.ini отсутствует"});
    }

    // reboot payload
    if (std::filesystem::exists("/atmosphere/reboot_payload.bin", ec)) {
        out.push_back({Severity::ok, "reboot_payload.bin", "найден"});
    } else {
        out.push_back({Severity::warn, "reboot_payload.bin",
                       "/atmosphere/reboot_payload.bin отсутствует"});
    }

    // Ryazhenka marker (best-effort)
    if (std::filesystem::exists("/switch/.ryazhenka", ec) ||
        std::filesystem::is_directory("/config/ryazhahand", ec)) {
        out.push_back({Severity::ok, "Ryazhenka", "обнаружены маркеры пака"});
    } else {
        out.push_back({Severity::warn, "Ryazhenka",
                       "маркеры пака не найдены (это нормально, если установлен чистый AMS)"});
    }

    return out;
}

Severity worst(const std::vector<Issue>& issues) {
    Severity s = Severity::ok;
    for (const auto& i : issues) {
        if (i.severity == Severity::error) return Severity::error;
        if (i.severity == Severity::warn) s = Severity::warn;
    }
    return s;
}

} // namespace ryazhenka::health

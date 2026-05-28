#include "ryazhenka_version_check.hpp"

#include <curl/curl.h>
#include <json.hpp>

#include <atomic>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <fstream>
#include <string>
#include <thread>
#include <vector>

#include "ryazhenka_config.hpp"
#include "ryazhenka_logger.hpp"

namespace ryazhenka::version_check {

namespace {

constexpr const char kToolsManifest[] = "romfs:/ryazhenka_tools.json";

// NRO header — only the size field matters for locating the asset section.
struct NroHeader {
    uint32_t pad0;
    uint32_t mod_offset;
    uint64_t pad1;
    uint32_t magic;   // "NRO0"
    uint32_t version;
    uint32_t size;    // distance from start to the AssetHeader
    uint32_t flags;
};
static_assert(sizeof(NroHeader) == 32);

struct AssetHeader {
    char magic[4]; // "ASET"
    uint32_t format_version;
    uint64_t icon_offset, icon_size;
    uint64_t nacp_offset, nacp_size;
    uint64_t romfs_offset, romfs_size;
};
static_assert(sizeof(AssetHeader) == 56);

constexpr std::size_t kNacpDisplayVersionOffset = 0x3060;
constexpr std::size_t kNacpDisplayVersionLength = 16;

size_t curlWriteToString(char* ptr, size_t size, size_t nmemb, void* userdata) {
    auto* out = static_cast<std::string*>(userdata);
    out->append(ptr, size * nmemb);
    return size * nmemb;
}

std::optional<std::string> fetchLatestTag(const std::string& url) {
    CURL* curl = curl_easy_init();
    if (!curl) return std::nullopt;

    std::string body;
    body.reserve(8192);

    struct curl_slist* headers = nullptr;
    headers = curl_slist_append(headers, "User-Agent: RyazhenkaUpdater/1.0");
    headers = curl_slist_append(headers, "Accept: application/vnd.github+json");

    curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
    curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
    curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
    curl_easy_setopt(curl, CURLOPT_CONNECTTIMEOUT, kCurlConnectTimeoutSec);
    curl_easy_setopt(curl, CURLOPT_TIMEOUT, 30L);
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, &curlWriteToString);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &body);

    const CURLcode rc = curl_easy_perform(curl);
    long http_code = 0;
    curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &http_code);
    curl_slist_free_all(headers);
    curl_easy_cleanup(curl);

    if (rc != CURLE_OK || http_code >= 400) {
        return std::nullopt;
    }

    try {
        const auto parsed = nlohmann::json::parse(body, nullptr, false);
        if (parsed.is_discarded()) return std::nullopt;
        if (auto it = parsed.find("tag_name"); it != parsed.end() && it->is_string()) {
            return it->get<std::string>();
        }
    } catch (...) {
        return std::nullopt;
    }
    return std::nullopt;
}

bool versionsDiffer(const std::string& local, const std::string& latest) {
    // Strip a leading "v" / "V" from either side before comparing — GitHub
    // tags often have it, NACP display_version rarely does.
    auto strip = [](std::string s) {
        if (!s.empty() && (s.front() == 'v' || s.front() == 'V')) s.erase(0, 1);
        return s;
    };
    return strip(local) != strip(latest);
}

} // namespace

std::optional<std::string> readNroDisplayVersion(const std::string& nroPath) {
    std::ifstream f(nroPath, std::ios::binary);
    if (!f.is_open()) return std::nullopt;

    NroHeader nro{};
    f.read(reinterpret_cast<char*>(&nro), sizeof(nro));
    if (!f) return std::nullopt;

    static constexpr uint32_t kNroMagic = 0x304F524E; // "NRO0" little-endian
    if (nro.magic != kNroMagic) return std::nullopt;

    AssetHeader asset{};
    f.seekg(nro.size, std::ios::beg);
    f.read(reinterpret_cast<char*>(&asset), sizeof(asset));
    if (!f) return std::nullopt;
    if (std::memcmp(asset.magic, "ASET", 4) != 0) return std::nullopt;
    if (asset.nacp_size < kNacpDisplayVersionOffset + kNacpDisplayVersionLength) {
        return std::nullopt;
    }

    char buf[kNacpDisplayVersionLength + 1] = {};
    f.seekg(static_cast<std::streamoff>(nro.size) + asset.nacp_offset + kNacpDisplayVersionOffset,
            std::ios::beg);
    f.read(buf, kNacpDisplayVersionLength);
    if (!f) return std::nullopt;

    std::string version(buf);
    while (!version.empty() && (version.back() == ' ' || version.back() == '\0')) {
        version.pop_back();
    }
    if (version.empty()) return std::nullopt;
    return version;
}

std::vector<ToolStatus> runChecks() {
    std::vector<ToolStatus> out;

    std::ifstream manifest(kToolsManifest);
    if (!manifest.is_open()) {
        ryazhenka::log::warn("version_check: cannot open romfs:/ryazhenka_tools.json");
        return out;
    }

    nlohmann::json root;
    try {
        manifest >> root;
    } catch (const std::exception& e) {
        ryazhenka::log::error(std::string("version_check: manifest parse: ") + e.what());
        return out;
    }

    auto appendCategory = [&](const char* category) {
        const auto it = root.find(category);
        if (it == root.end() || !it->is_array()) return;
        for (const auto& entry : *it) {
            ToolStatus status;
            status.name = entry.value("name", std::string{"?"});
            status.repo = entry.value("repo", std::string{});
            status.local_path = entry.value("local_path", std::string{});
            const std::string api_url = entry.value("api_url", std::string{});

            if (!status.local_path.empty()) {
                status.local_version = readNroDisplayVersion(status.local_path);
                if (!status.local_version) {
                    status.note = "not installed or unreadable NACP";
                }
            }
            if (!api_url.empty()) {
                status.latest_version = fetchLatestTag(api_url);
                if (!status.latest_version && status.note.empty()) {
                    status.note = "GitHub API unreachable";
                }
            }
            if (status.local_version && status.latest_version) {
                status.update_available =
                    versionsDiffer(*status.local_version, *status.latest_version);
            }
            out.push_back(std::move(status));
        }
    };

    appendCategory("tools");
    appendCategory("overlays");
    appendCategory("sysmodules");

    return out;
}

void scheduleBackgroundCheck() {
    static std::atomic_flag s_started = ATOMIC_FLAG_INIT;
    if (s_started.test_and_set()) return;

    std::thread([]() {
        ryazhenka::log::info("version_check: starting background scan");
        const auto results = runChecks();
        for (const auto& r : results) {
            std::string line = r.name;
            line += "  local=" + r.local_version.value_or("?");
            line += "  latest=" + r.latest_version.value_or("?");
            if (r.update_available) line += "  [UPDATE]";
            if (!r.note.empty()) line += "  (" + r.note + ")";
            ryazhenka::log::info(line);
        }
        ryazhenka::log::info("version_check: scan complete");
    }).detach();
}

} // namespace ryazhenka::version_check

#include "ryazhenka_banner.hpp"

#include <atomic>
#include <chrono>
#include <cstdio>
#include <filesystem>
#include <mutex>
#include <thread>

#include <curl/curl.h>
#include <json.hpp>

#include "constants.hpp"
#include "fs.hpp"
#include "ryazhenka_config.hpp"
#include "ryazhenka_curl_retry.hpp"
#include "ryazhenka_logger.hpp"

namespace fs_ns = std::filesystem;

namespace ryazhenka::banner {

namespace {

std::atomic<bool> g_fetchInFlight{false};
std::mutex g_diskMutex;  // serialises writes to the cache file

std::size_t writeToFile(void* ptr, std::size_t size, std::size_t nmemb, void* userdata) {
    auto* f = static_cast<std::FILE*>(userdata);
    return std::fwrite(ptr, size, nmemb, f);
}

std::size_t writeToString(void* ptr, std::size_t size, std::size_t nmemb, void* userdata) {
    auto* s = static_cast<std::string*>(userdata);
    s->append(static_cast<const char*>(ptr), size * nmemb);
    return size * nmemb;
}

// Best-effort: parse "tag_name" and "body" from the GitHub release feed and
// write each to its own small cache file next to the banner. Silent on
// failure — the banner refresh succeeded either way.
void fetchAndWritePackTagAndNotes() {
    CURL* curl = curl_easy_init();
    if (!curl)
        return;

    std::string body;
    body.reserve(4096);

    struct curl_slist* headers = nullptr;
    headers = curl_slist_append(headers, "User-Agent: RyazhenkaUpdater/1.0");
    headers = curl_slist_append(headers, "Accept: application/vnd.github+json");

    curl_easy_setopt(curl, CURLOPT_URL, kSigpatchesReleasesUrl);
    curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
    curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
    curl_easy_setopt(curl, CURLOPT_CONNECTTIMEOUT, kCurlConnectTimeoutSec);
    curl_easy_setopt(curl, CURLOPT_TIMEOUT, 20L);
    curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 0L);
    curl_easy_setopt(curl, CURLOPT_SSL_VERIFYHOST, 0L);
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, &writeToString);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &body);

    long http_code = 0;
    const CURLcode rc = curl::performWithRetry(curl, &http_code);
    curl_slist_free_all(headers);
    curl_easy_cleanup(curl);

    if (rc != CURLE_OK || http_code >= 400)
        return;

    try {
        auto json = nlohmann::json::parse(body, nullptr, /*allow_exceptions=*/false);
        if (!json.is_object())
            return;

        if (json.contains("tag_name") && json["tag_name"].is_string()) {
            const std::string tag = json["tag_name"].get<std::string>();
            if (!tag.empty()) {
                if (std::FILE* f = std::fopen(kPackTagCachePath, "w")) {
                    std::fwrite(tag.data(), 1, tag.size(), f);
                    std::fclose(f);
                    log::info(std::string("banner: cached pack tag ") + tag);
                }
            }
        }

        if (json.contains("body") && json["body"].is_string()) {
            const std::string notes = json["body"].get<std::string>();
            // Allow empty (some releases ship without notes) — just write
            // whatever GitHub gave us, capped to ~16 KiB so a stray giant
            // release body can't blow up the SD cache file.
            constexpr std::size_t kMaxNotes = 16 * 1024;
            const std::string clipped = notes.size() > kMaxNotes
                                            ? notes.substr(0, kMaxNotes)
                                            : notes;
            if (std::FILE* f = std::fopen(kPackNotesCachePath, "w")) {
                std::fwrite(clipped.data(), 1, clipped.size(), f);
                std::fclose(f);
            }
        }
    } catch (...) {
        // best effort
    }
}

}  // namespace

std::string cachedPath() {
    std::error_code ec;
    if (fs_ns::exists(kBannerCachePath, ec))
        return kBannerCachePath;
    return "";
}

bool cacheIsFresh() {
    std::error_code ec;
    if (!fs_ns::exists(kBannerCachePath, ec))
        return false;
    const auto mtime = fs_ns::last_write_time(kBannerCachePath, ec);
    if (ec)
        return false;
    const auto now = decltype(mtime)::clock::now();
    const auto ageHours =
        std::chrono::duration_cast<std::chrono::hours>(now - mtime).count();
    return ageHours < kBannerCacheTtlHours;
}

bool fetchNow() {
    std::lock_guard<std::mutex> lk(g_diskMutex);

    fs::createTree(CONFIG_PATH);
    const std::string tmpPath = std::string(kBannerCachePath) + ".part";

    std::FILE* f = std::fopen(tmpPath.c_str(), "wb");
    if (!f) {
        log::warn("banner: cannot open tmp file for write");
        return false;
    }

    CURL* curl = curl_easy_init();
    if (!curl) {
        std::fclose(f);
        return false;
    }

    curl_easy_setopt(curl, CURLOPT_URL, kBannerUrl);
    curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
    curl_easy_setopt(curl, CURLOPT_CONNECTTIMEOUT, kCurlConnectTimeoutSec);
    curl_easy_setopt(curl, CURLOPT_TIMEOUT, 30L);
    curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 0L);
    curl_easy_setopt(curl, CURLOPT_SSL_VERIFYHOST, 0L);
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, &writeToFile);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, f);

    long http_code = 0;
    const CURLcode rc = curl::performWithRetry(curl, &http_code);
    curl_easy_cleanup(curl);
    std::fclose(f);

    if (rc != CURLE_OK || http_code >= 400) {
        log::warn(std::string("banner: fetch failed (curl ") + std::to_string(rc) +
                  ", http " + std::to_string(http_code) + ")");
        std::error_code ec;
        fs_ns::remove(tmpPath, ec);
        return false;
    }

    std::error_code ec;
    fs_ns::rename(tmpPath, kBannerCachePath, ec);
    if (ec) {
        log::warn("banner: rename failed");
        return false;
    }
    log::info("banner: cached fresh copy");

    // Same release, so refresh the cached pack tag + notes in the same pass —
    // one round-trip for everything the install/notes cards need.
    fetchAndWritePackTagAndNotes();
    return true;
}

bool refreshAsync() {
    bool expected = false;
    if (!g_fetchInFlight.compare_exchange_strong(expected, true))
        return false;
    std::thread([] {
        try {
            fetchNow();
        } catch (...) {
            log::warn("banner: async fetch threw");
        }
        g_fetchInFlight.store(false);
    }).detach();
    return true;
}

brls::Image* makeImage() {
    const std::string path = cachedPath();
    if (!cacheIsFresh())
        refreshAsync();
    if (path.empty())
        return nullptr;
    return new brls::Image(path);
}

namespace {

std::string slurp(const char* path) {
    std::error_code ec;
    if (!fs_ns::exists(path, ec))
        return "";
    std::FILE* f = std::fopen(path, "r");
    if (!f)
        return "";
    std::string out;
    char buf[4096];
    while (std::size_t n = std::fread(buf, 1, sizeof(buf), f))
        out.append(buf, n);
    std::fclose(f);
    return out;
}

}  // namespace

std::string cachedPackTag() {
    std::string out = slurp(kPackTagCachePath);
    // Strip stray whitespace / newlines for the small tag string.
    while (!out.empty() && (out.back() == '\n' || out.back() == '\r' || out.back() == ' '))
        out.pop_back();
    return out;
}

std::string cachedPackNotes() {
    return slurp(kPackNotesCachePath);
}

}  // namespace ryazhenka::banner

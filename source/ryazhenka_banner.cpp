#include "ryazhenka_banner.hpp"

#include <atomic>
#include <chrono>
#include <cstdio>
#include <filesystem>
#include <mutex>
#include <thread>

#include <curl/curl.h>

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

}  // namespace ryazhenka::banner

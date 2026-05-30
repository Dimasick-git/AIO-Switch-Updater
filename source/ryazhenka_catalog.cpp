#include "ryazhenka_catalog.hpp"

#include <atomic>
#include <cstdio>
#include <filesystem>
#include <mutex>
#include <string>
#include <thread>

#include <curl/curl.h>

#include "constants.hpp"
#include "fs.hpp"
#include "ryazhenka_config.hpp"
#include "ryazhenka_curl_retry.hpp"
#include "ryazhenka_logger.hpp"

namespace fs_ns = std::filesystem;

namespace ryazhenka::catalog {

namespace {

constexpr const char kCachePath[] = "/config/aio-switch-updater/nx-links.json";

std::atomic<bool> g_fetchInFlight{false};
std::atomic<bool> g_shuttingDown{false};
std::mutex g_diskMutex;

std::size_t writeToString(void* ptr, std::size_t size, std::size_t nmemb, void* userdata) {
    auto* s = static_cast<std::string*>(userdata);
    s->append(static_cast<const char*>(ptr), size * nmemb);
    return size * nmemb;
}

bool fetchNowToCache() {
    if (g_shuttingDown.load()) return false;
    std::lock_guard<std::mutex> lk(g_diskMutex);
    if (g_shuttingDown.load()) return false;

    CURL* curl = curl_easy_init();
    if (!curl) return false;

    std::string body;
    body.reserve(32 * 1024);

    curl_easy_setopt(curl, CURLOPT_URL, NXLINKS_URL);
    curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
    curl_easy_setopt(curl, CURLOPT_CONNECTTIMEOUT, kCurlConnectTimeoutSec);
    curl_easy_setopt(curl, CURLOPT_TIMEOUT, 30L);
    curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 0L);
    curl_easy_setopt(curl, CURLOPT_SSL_VERIFYHOST, 0L);
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, &writeToString);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &body);

    long http = 0;
    const CURLcode rc = curl::performWithRetry(curl, &http);
    curl_easy_cleanup(curl);

    if (rc != CURLE_OK || http >= 400 || body.empty()) {
        log::warn(std::string("catalog: fetch failed (curl ") + std::to_string(rc) +
                  ", http " + std::to_string(http) + ")");
        return false;
    }

    try {
        auto parsed = nlohmann::ordered_json::parse(body, nullptr, /*allow_exceptions=*/false);
        if (parsed.is_discarded() || !parsed.is_object()) {
            log::warn("catalog: response is not a JSON object");
            return false;
        }
    } catch (...) {
        return false;
    }

    fs::createTree(CONFIG_PATH);
    const std::string tmpPath = std::string(kCachePath) + ".part";
    if (std::FILE* f = std::fopen(tmpPath.c_str(), "wb")) {
        std::fwrite(body.data(), 1, body.size(), f);
        std::fclose(f);
        std::error_code ec;
        fs_ns::rename(tmpPath, kCachePath, ec);
        if (!ec) {
            log::info("catalog: nx-links cache refreshed");
            return true;
        }
    }
    return false;
}

}  // namespace

nlohmann::ordered_json cachedNxLinks() {
    std::error_code ec;
    if (!fs_ns::exists(kCachePath, ec))
        return nlohmann::ordered_json::object();

    std::FILE* f = std::fopen(kCachePath, "r");
    if (!f)
        return nlohmann::ordered_json::object();
    std::string body;
    char buf[4096];
    while (std::size_t n = std::fread(buf, 1, sizeof(buf), f))
        body.append(buf, n);
    std::fclose(f);

    try {
        auto json = nlohmann::ordered_json::parse(body, nullptr, /*allow_exceptions=*/false);
        if (!json.is_discarded() && json.is_object())
            return json;
    } catch (...) {}
    return nlohmann::ordered_json::object();
}

void writeNxLinksCache(const nlohmann::ordered_json& json) {
    if (!json.is_object() || json.empty())
        return;
    std::lock_guard<std::mutex> lk(g_diskMutex);
    fs::createTree(CONFIG_PATH);
    const std::string body = json.dump();
    const std::string tmpPath = std::string(kCachePath) + ".part";
    if (std::FILE* f = std::fopen(tmpPath.c_str(), "wb")) {
        std::fwrite(body.data(), 1, body.size(), f);
        std::fclose(f);
        std::error_code ec;
        fs_ns::rename(tmpPath, kCachePath, ec);
    }
}

bool refreshAsync() {
    if (g_shuttingDown.load()) return false;
    bool expected = false;
    if (!g_fetchInFlight.compare_exchange_strong(expected, true))
        return false;
    std::thread([] {
        try {
            fetchNowToCache();
        } catch (...) {
            log::warn("catalog: async fetch threw");
        }
        g_fetchInFlight.store(false);
    }).detach();
    return true;
}

void signalShutdown() {
    g_shuttingDown.store(true);
}

}  // namespace ryazhenka::catalog

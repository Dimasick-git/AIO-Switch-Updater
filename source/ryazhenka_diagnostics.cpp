/**
 * @file ryazhenka_diagnostics.cpp
 * @brief See ryazhenka_diagnostics.hpp.
 * @author Dimasick-git
 */

#include "ryazhenka_diagnostics.hpp"

#include <curl/curl.h>

#include <chrono>
#include <cstdio>
#include <ctime>
#include <deque>
#include <fstream>
#include <functional>
#include <sstream>
#include <string>
#include <system_error>
#include <thread>
#include <utility>
#include <vector>

#include "ryazhenka_config.hpp"
#include "ryazhenka_logger.hpp"
#include "ryazhenka_system_info.hpp"
#include "ryazhenka_version_check.hpp"

namespace ryazhenka::diagnostics {

namespace {

std::string utcTimestamp() {
    using namespace std::chrono;
    const auto now = system_clock::now();
    const auto t = system_clock::to_time_t(now);
    std::tm tm{};
    gmtime_r(&t, &tm);
    char buf[64];
    std::snprintf(buf, sizeof buf, "%04d%02d%02dT%02d%02d%02dZ",
                  tm.tm_year + 1900, tm.tm_mon + 1, tm.tm_mday,
                  tm.tm_hour, tm.tm_min, tm.tm_sec);
    return buf;
}

std::size_t curlDiscardBody(char*, std::size_t size, std::size_t nmemb, void*) {
    return size * nmemb;
}

} // namespace

std::vector<std::string> tailLog(std::size_t lines) {
    std::vector<std::string> out;
    std::ifstream f(kLogFilePath);
    if (!f.is_open()) return out;

    std::deque<std::string> ring;
    std::string line;
    while (std::getline(f, line)) {
        ring.push_back(line);
        if (ring.size() > lines) ring.pop_front();
    }
    out.assign(ring.begin(), ring.end());
    return out;
}

std::string buildDumpText() {
    std::ostringstream ss;
    ss << "Ryazhenka Updater — diagnostic dump\n";
    ss << "Generated (UTC): " << utcTimestamp() << "\n\n";

    ss << "## sysinfo\n";
    ss << ryazhenka::sysinfo::formatOneLine(ryazhenka::sysinfo::collect()) << "\n\n";

    ss << "## version_check\n";
    const auto checks = ryazhenka::version_check::runChecks();
    if (checks.empty()) {
        ss << "(no manifest entries or no network)\n";
    } else {
        for (const auto& r : checks) {
            ss << "- " << r.name
               << "  local=" << r.local_version.value_or("?")
               << "  latest=" << r.latest_version.value_or("?");
            if (r.update_available) ss << "  [UPDATE]";
            if (!r.note.empty()) ss << "  (" << r.note << ")";
            ss << "\n";
        }
    }
    ss << "\n";

    ss << "## log tail (last 200 lines)\n";
    const auto lines = tailLog(200);
    if (lines.empty()) {
        ss << "(empty — enable \"verbose_log\": true in config.json)\n";
    } else {
        for (const auto& line : lines) ss << line << "\n";
    }

    return ss.str();
}

std::string writeDumpFile() {
    const std::string path = std::string("/switch/aio-switch-updater/diag-") +
                             utcTimestamp() + ".txt";
    std::ofstream f(path);
    if (!f.is_open()) {
        ryazhenka::log::error("diagnostics: cannot open " + path + " for writing");
        return {};
    }
    f << buildDumpText();
    if (!f) {
        ryazhenka::log::error("diagnostics: write failure on " + path);
        return {};
    }
    ryazhenka::log::info("diagnostics: wrote " + path);
    return path;
}

namespace {

ProbeResult probeOne(const std::string& url) {
    ProbeResult r;
    r.host = url;

    CURL* curl = curl_easy_init();
    if (!curl) return r;

    struct curl_slist* hdr = curl_slist_append(nullptr,
                                               "User-Agent: RyazhenkaUpdater/1.0");
    curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
    curl_easy_setopt(curl, CURLOPT_NOBODY, 1L);
    curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
    // Tight timeouts so a dead host cannot pin the UI thread; the original
    // 10s+10s × 5 hosts could lock the popup for 50+ seconds and B did
    // nothing while curl was inside the syscall.
    curl_easy_setopt(curl, CURLOPT_CONNECTTIMEOUT, 3L);
    curl_easy_setopt(curl, CURLOPT_TIMEOUT, 5L);
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, &curlDiscardBody);
    curl_easy_setopt(curl, CURLOPT_HTTPHEADER, hdr);

    const auto t0 = std::chrono::steady_clock::now();
    const CURLcode rc = curl_easy_perform(curl);
    const auto t1 = std::chrono::steady_clock::now();

    r.latency_ms = static_cast<long>(
        std::chrono::duration_cast<std::chrono::milliseconds>(t1 - t0).count());
    curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &r.http_code);
    r.ok = (rc == CURLE_OK && r.http_code > 0 && r.http_code < 400);

    curl_slist_free_all(hdr);
    curl_easy_cleanup(curl);
    return r;
}

const std::vector<std::string> kProbeTargets = {
    "https://api.github.com",
    "https://raw.githubusercontent.com",
    "https://github.com/Dimasick-git/Ryzhenka",
    "https://github.com/HamletDuFromage/aio-switch-updater",
    "https://t.me/Ryazhenkacfw",
};

} // anonymous

std::vector<ProbeResult> runNetworkProbe() {
    std::vector<ProbeResult> results;
    results.reserve(kProbeTargets.size());
    for (const auto& url : kProbeTargets) {
        results.push_back(probeOne(url));
    }
    return results;
}

void runNetworkProbeAsync(std::function<void(std::vector<ProbeResult>)> onDone) {
    std::thread([onDone = std::move(onDone)]() {
        std::vector<ProbeResult> results;
        results.reserve(kProbeTargets.size());
        try {
            for (const auto& url : kProbeTargets) {
                results.push_back(probeOne(url));
            }
        } catch (const std::exception& e) {
            ryazhenka::log::warn(std::string("net_diag thread: ") + e.what());
        } catch (...) {
            ryazhenka::log::warn("net_diag thread: unknown exception");
        }
        if (onDone) onDone(std::move(results));
    }).detach();
}

} // namespace ryazhenka::diagnostics

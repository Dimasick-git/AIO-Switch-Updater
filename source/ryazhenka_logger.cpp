#include "ryazhenka_logger.hpp"

#include <borealis.hpp>
#include <chrono>
#include <cstdio>
#include <ctime>
#include <filesystem>
#include <mutex>
#include <string>
#include <system_error>

#include "ryazhenka_config.hpp"

namespace ryazhenka::log {

namespace {

std::mutex g_mutex;
bool g_file_enabled = false;
bool g_initialized = false;

const char* levelTag(Level level) {
    switch (level) {
        case Level::trace: return "TRACE";
        case Level::debug: return "DEBUG";
        case Level::info:  return "INFO";
        case Level::warn:  return "WARN";
        case Level::error: return "ERROR";
    }
    return "?";
}

std::string nowIso8601() {
    using namespace std::chrono;
    const auto now = system_clock::now();
    const auto t = system_clock::to_time_t(now);
    const auto ms = duration_cast<milliseconds>(now.time_since_epoch()) % 1000;
    std::tm tm{};
#ifdef _WIN32
    gmtime_s(&tm, &t);
#else
    gmtime_r(&t, &tm);
#endif
    char buf[64];
    std::snprintf(buf, sizeof buf, "%04d-%02d-%02dT%02d:%02d:%02d.%03dZ",
                  tm.tm_year + 1900, tm.tm_mon + 1, tm.tm_mday,
                  tm.tm_hour, tm.tm_min, tm.tm_sec,
                  static_cast<int>(ms.count()));
    return buf;
}

// Rotate kLogFilePath → kLogFilePath.1 → … → .N when the active log exceeds
// kLogRotateBytes. Best-effort: any std::filesystem error is swallowed so the
// logger never crashes the host app.
void rotateIfNeeded() {
    std::error_code ec;
    const auto size = std::filesystem::file_size(kLogFilePath, ec);
    if (ec || size < kLogRotateBytes) {
        return;
    }

    for (int i = kLogRotateKeep; i >= 1; --i) {
        std::string from = std::string(kLogFilePath) + (i == 1 ? "" : "." + std::to_string(i - 1));
        std::string to   = std::string(kLogFilePath) + "." + std::to_string(i);
        std::filesystem::remove(to, ec);
        std::filesystem::rename(from, to, ec);
    }
}

void writeToFile(Level level, std::string_view message) {
    rotateIfNeeded();
    std::FILE* f = std::fopen(kLogFilePath, "a");
    if (!f) return;
    std::fprintf(f, "%s [%s] %.*s\n",
                 nowIso8601().c_str(),
                 levelTag(level),
                 static_cast<int>(message.size()),
                 message.data());
    std::fclose(f);
}

} // namespace

void init() {
    std::lock_guard lock(g_mutex);
    if (g_initialized) return;
    g_initialized = true;

    // File logging is opt-in via a key in the existing config.json. Borealis
    // already logs to stdout/nxlink, so this only adds the SD mirror.
    std::error_code ec;
    if (std::filesystem::exists("/config/aio-switch-updater/config.json", ec)) {
        // Lazy: just check whether the user wrote "verbose_log": true. We don't
        // parse JSON here to keep the logger dependency-free; instead the file
        // is grep-style scanned, which is good enough for a single boolean.
        std::FILE* f = std::fopen("/config/aio-switch-updater/config.json", "r");
        if (f) {
            char buf[4096];
            const auto n = std::fread(buf, 1, sizeof(buf) - 1, f);
            buf[n] = '\0';
            std::fclose(f);
            const std::string_view view(buf, n);
            if (view.find(kLogConfigKey) != std::string_view::npos &&
                view.find("true") != std::string_view::npos) {
                g_file_enabled = true;
            }
        }
    }
}

void write(Level level, std::string_view message) {
    std::lock_guard lock(g_mutex);

    // brls::Logger forwards to fmt::print, which uses {} format strings — not
    // printf-style %.*s. The previous %.*s format was throwing inside
    // fmt::vformat on every call and being silently swallowed by the try/catch
    // in brls::Logger::log, so every ryazhenka log line disappeared from the
    // borealis console (only the SD file mirror survived).
    const std::string text(message);  // fmt's string_view formatter is OK, but
                                       // building an owned string avoids any
                                       // dangling-view surprise across the call.
    switch (level) {
        case Level::trace:
        case Level::debug:
            brls::Logger::debug("[ryazhenka] {}", text);
            break;
        case Level::info:
            brls::Logger::info("[ryazhenka] {}", text);
            break;
        case Level::warn:
            brls::Logger::warning("[ryazhenka] {}", text);
            break;
        case Level::error:
            brls::Logger::error("[ryazhenka] {}", text);
            break;
    }

    if (g_file_enabled) {
        writeToFile(level, message);
    }
}

} // namespace ryazhenka::log

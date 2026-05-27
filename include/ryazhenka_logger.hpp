#pragma once

// Lightweight structured logger that mirrors writes to a rotated file on the
// SD card while still going through brls::Logger so the borealis console keeps
// working. File output is gated on the "verbose_log" key in the existing
// /config/aio-switch-updater/config.json.

#include <string>
#include <string_view>

namespace ryazhenka::log {

enum class Level {
    trace,
    debug,
    info,
    warn,
    error,
};

void init();

void write(Level level, std::string_view message);

inline void trace(std::string_view m) { write(Level::trace, m); }
inline void debug(std::string_view m) { write(Level::debug, m); }
inline void info(std::string_view m)  { write(Level::info,  m); }
inline void warn(std::string_view m)  { write(Level::warn,  m); }
inline void error(std::string_view m) { write(Level::error, m); }

} // namespace ryazhenka::log

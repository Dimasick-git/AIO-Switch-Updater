#pragma once

/**
 * @file ryazhenka_diagnostics.hpp
 * @brief End-user diagnostics: log tail, bug-report dump, network probe.
 * @author Dimasick-git
 *
 * All entry points are safe to call from the UI thread — network probe blocks
 * for at most kCurlConnectTimeoutSec * targets, log tail is bounded by
 * `lines` parameter, and writeDumpFile silently no-ops if the SD path is not
 * writable.
 */

#include <cstddef>
#include <string>
#include <vector>

namespace ryazhenka::diagnostics {

/// Return the last `lines` lines of /config/aio-switch-updater/log.txt.
/// Empty if no log file or file mirroring is off (verbose_log=false).
std::vector<std::string> tailLog(std::size_t lines = 200);

/// Build a multi-section text report suitable for pasting into a GitHub issue.
std::string buildDumpText();

/// Save buildDumpText() to /switch/aio-switch-updater/diag-<UTC>.txt.
/// Returns the resulting path or empty string on failure.
std::string writeDumpFile();

struct ProbeResult {
    std::string host;
    long http_code = 0;
    long latency_ms = 0;
    bool ok = false;
};

/// HEAD requests to GitHub / raw / Telegram / pack repo / upstream. Sequential
/// (the libcurl in libnx is built without async support on Switch).
std::vector<ProbeResult> runNetworkProbe();

} // namespace ryazhenka::diagnostics

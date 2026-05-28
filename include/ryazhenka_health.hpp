#pragma once

/**
 * @file ryazhenka_health.hpp
 * @brief Lightweight CFW health probe: spots missing or broken pieces of an
 *        Atmosphere install so the user can act before the next boot.
 * @author Dimasick-git
 */

#include <string>
#include <vector>

namespace ryazhenka::health {

enum class Severity {
    ok,
    warn,
    error,
};

struct Issue {
    Severity severity;
    std::string title;       // i18n-friendly key or already-localized text
    std::string detail;      // free-form human note (paths, counts, etc.)
};

/// Run all checks once. Pure filesystem work, no network, no libnx calls.
/// Safe to invoke from the UI thread.
std::vector<Issue> run();

/// Helper for callers that want a single severity for "all good or not".
Severity worst(const std::vector<Issue>& issues);

/// Run health::run() and, if the worst severity is WARN or ERROR, surface
/// a brls::Application::notify() toast so the user knows to check the
/// dashboard. No-op when ryazhenka::kAutoHealthAfterActions is false.
/// Safe to call from a brls worker thread — notify() is queued.
void runAndNotifyIfDegraded();

} // namespace ryazhenka::health

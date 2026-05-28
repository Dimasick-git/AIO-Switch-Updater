#pragma once

/**
 * @file ryazhenka_sigpatches.hpp
 * @brief Detect outdated /atmosphere/exefs_patches/ relative to the
 *        latest Ryazhenka pack release.
 *
 * Local side: SHA-256 over the sorted (filename, size, mtime) triples
 * of every file under /atmosphere/exefs_patches/. This is a
 * fingerprint that changes whenever sigpatches are reinstalled.
 *
 * Remote side: GET kSigpatchesReleasesUrl, parse `published_at`
 * (ISO-8601). Cached on SD for kSigpatchesCacheTtlHours so we don't
 * burn GitHub anonymous rate-limit on every health check.
 *
 * @author Dimasick-git
 */

#include <optional>
#include <string>

#include "ryazhenka_health.hpp"

namespace ryazhenka::sigpatches {

/// Hex SHA-256 fingerprint of the current /atmosphere/exefs_patches/
/// content. Empty if the directory is missing or unreadable.
std::string localFingerprint();

/// Maximum filesystem mtime under /atmosphere/exefs_patches/, expressed
/// as the number of days since "now" (rounded down). Empty optional if
/// the directory is missing.
std::optional<int> daysSinceLocalUpdate();

/// Days since the latest remote sigpatches release. Network-bound;
/// caches the JSON for kSigpatchesCacheTtlHours so subsequent calls
/// are cheap. Empty optional on transport/parse failure.
std::optional<int> daysSinceRemoteRelease();

/// Combine local + remote into a single Issue suitable for appending
/// to ryazhenka::health::run() output.
health::Issue evaluate();

} // namespace ryazhenka::sigpatches

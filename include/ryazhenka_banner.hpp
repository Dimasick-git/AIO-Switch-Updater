#pragma once

/**
 * @file ryazhenka_banner.hpp
 * @brief Per-release banner (bbootlogo.png) — lazy fetch + cache + display.
 * @author Dimasick-git
 *
 * The banner is the per-release hero image that ships alongside the pack on the
 * Ryzhenka GitHub release. Downloaded lazily on first use, cached to SD, and
 * refreshed in the background when older than kBannerCacheTtlHours.
 */

#include <borealis.hpp>
#include <string>

namespace ryazhenka::banner {

/// Path to the cached banner on SD (empty if the cache file is missing). Does
/// not perform any network I/O.
std::string cachedPath();

/// True if cachedPath() is fresh (younger than kBannerCacheTtlHours).
bool cacheIsFresh();

/// Synchronously fetches the banner to the cache. Safe to call from a
/// worker page; returns false on failure. Honours the curl retry policy.
bool fetchNow();

/// Kicks off a detached background fetch (no callback). No-op if a fetch is
/// already in flight; safe to call multiple times. Returns true if a fetch was
/// scheduled.
bool refreshAsync();

/// Convenience: returns a brls::Image* for the cached banner, or nullptr if
/// the cache is missing. Triggers refreshAsync() when the cache is stale so a
/// fresh image is on disk for next launch.
brls::Image* makeImage();

/// Same as makeImage() but NEVER spawns a refresh thread. Safe to call from a
/// view constructor that runs as part of MainFrame construction (and therefore
/// before the first frame) — the startup-crash rule from commit ca62519
/// forbids any detached-thread + curl pattern from the launch path. Returns
/// nullptr when the cache is missing.
brls::Image* makeCachedOnlyImage();

/// Latest pack release tag (e.g. "v7.2.1"), or empty string if not yet
/// cached. Refreshed alongside the banner image — no separate network call.
std::string cachedPackTag();

/// Latest pack release notes (GitHub release `body`), or empty string if not
/// yet cached. May be markdown; the UI renders it as plain text.
std::string cachedPackNotes();

}  // namespace ryazhenka::banner

#pragma once

/**
 * @file ryazhenka_catalog.hpp
 * @brief SD-cached mirror of the nx-links catalogue JSON.
 * @author Dimasick-git
 *
 * MainFrame's constructor previously did a blocking `download::getRequest`
 * for NXLINKS_URL, with a worst-case wait of ~30 s on a flaky network —
 * exactly the "бесконечная загрузка" the user is reporting. The cache here
 * lets MainFrame come up instantly from the previous launch's snapshot,
 * with a background refresh for next time. Pure file I/O — never spawns
 * a thread from a startup-path caller.
 */

#include <json.hpp>
#include <string>

namespace ryazhenka::catalog {

/// Returns the cached nxlinks JSON, or an empty object if the cache is
/// missing / corrupt. Pure filesystem; no network, no thread spawn — safe
/// to call from MainFrame's constructor.
nlohmann::ordered_json cachedNxLinks();

/// Writes the given JSON to the SD cache. Safe to call from any context;
/// silent on failure.
void writeNxLinksCache(const nlohmann::ordered_json& json);

/// Kicks off a detached background fetch of NXLINKS_URL and writes the
/// result to the cache. MUST NOT be called from the startup path — wire
/// it to the Settings "refresh catalog" button or fire it from a deferred
/// hook well after the first frame.
bool refreshAsync();

/// Tells any in-flight refresh worker to bail before its next curl step.
/// Call from main() right after the brls main loop exits and BEFORE
/// curl_global_cleanup(). Idempotent.
void signalShutdown();

}  // namespace ryazhenka::catalog

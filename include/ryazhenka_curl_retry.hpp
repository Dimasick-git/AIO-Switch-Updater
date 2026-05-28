#pragma once

// Header-only retry helper around curl_easy_perform. Intended for use by code
// originating in this fork — the upstream source/download.cpp is left intact
// so daily sync stays low-conflict. Retries are gated on transient errors
// (DNS / timeout / partial reads / HTTP 5xx) with exponential backoff capped
// by ryazhenka::kCurlMaxDelayMs.

#include <curl/curl.h>

#include <chrono>
#include <string>
#include <thread>

#include "ryazhenka_config.hpp"
#include "ryazhenka_logger.hpp"

namespace ryazhenka::curl {

inline bool isTransient(CURLcode rc, long http_code) {
    if (http_code >= 500 && http_code <= 599) return true;
    switch (rc) {
        case CURLE_OPERATION_TIMEDOUT:
        case CURLE_COULDNT_RESOLVE_HOST:
        case CURLE_COULDNT_CONNECT:
        case CURLE_GOT_NOTHING:
        case CURLE_RECV_ERROR:
        case CURLE_SEND_ERROR:
        case CURLE_PARTIAL_FILE:
            return true;
        default:
            return false;
    }
}

// Returns the last CURLcode observed. The HTTP status of the final attempt is
// written to *out_http when non-null.
inline CURLcode performWithRetry(CURL* curl, long* out_http = nullptr) {
    CURLcode last = CURLE_OK;
    long http = 0;

    for (int attempt = 0; attempt <= kCurlMaxRetries; ++attempt) {
        last = curl_easy_perform(curl);
        curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &http);
        if (out_http) *out_http = http;

        if (last == CURLE_OK && http < 500) {
            return last;
        }
        if (attempt == kCurlMaxRetries) break;
        if (!isTransient(last, http)) {
            return last;
        }

        int delay = kCurlBaseDelayMs * (1 << attempt);
        if (delay > kCurlMaxDelayMs) delay = kCurlMaxDelayMs;
        ryazhenka::log::warn(
            std::string("curl retry attempt ") + std::to_string(attempt + 1) +
            " (rc=" + std::to_string(static_cast<int>(last)) +
            " http=" + std::to_string(http) +
            ") in " + std::to_string(delay) + "ms");
        std::this_thread::sleep_for(std::chrono::milliseconds(delay));
    }
    return last;
}

} // namespace ryazhenka::curl

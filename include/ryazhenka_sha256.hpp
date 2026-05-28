#pragma once

/**
 * @file ryazhenka_sha256.hpp
 * @brief Header-only SHA-256 helper backed by mbedtls (already linked).
 * @author Dimasick-git
 *
 * No external state, no logging side effects — safe to inline freely. Returns
 * empty string on any I/O or mbedtls failure so callers can branch cleanly.
 */

#include <mbedtls/sha256.h>

#include <array>
#include <cctype>
#include <cstdint>
#include <cstdio>
#include <fstream>
#include <string>
#include <vector>

namespace ryazhenka::sha256 {

inline std::string toHex(const std::array<std::uint8_t, 32>& bytes) {
    static constexpr char kHexDigits[] = "0123456789abcdef";
    std::string out;
    out.resize(64);
    for (std::size_t i = 0; i < bytes.size(); ++i) {
        out[2 * i]     = kHexDigits[bytes[i] >> 4];
        out[2 * i + 1] = kHexDigits[bytes[i] & 0x0F];
    }
    return out;
}

inline std::string ofFile(const std::string& path) {
    std::ifstream f(path, std::ios::binary);
    if (!f.is_open()) return {};

    mbedtls_sha256_context ctx;
    mbedtls_sha256_init(&ctx);
    mbedtls_sha256_starts(&ctx, /*is224=*/0);

    constexpr std::size_t kBuf = 64 * 1024;
    std::vector<char> buf(kBuf);
    while (f) {
        f.read(buf.data(), static_cast<std::streamsize>(buf.size()));
        const auto got = static_cast<std::size_t>(f.gcount());
        if (got == 0) break;
        mbedtls_sha256_update(&ctx,
                              reinterpret_cast<const unsigned char*>(buf.data()),
                              got);
    }

    std::array<std::uint8_t, 32> digest{};
    mbedtls_sha256_finish(&ctx, digest.data());
    mbedtls_sha256_free(&ctx);
    return toHex(digest);
}

inline std::string ofBuffer(const std::string& data) {
    mbedtls_sha256_context ctx;
    mbedtls_sha256_init(&ctx);
    mbedtls_sha256_starts(&ctx, /*is224=*/0);
    mbedtls_sha256_update(
        &ctx,
        reinterpret_cast<const unsigned char*>(data.data()),
        data.size());
    std::array<std::uint8_t, 32> digest{};
    mbedtls_sha256_finish(&ctx, digest.data());
    mbedtls_sha256_free(&ctx);
    return toHex(digest);
}

inline bool matches(const std::string& path, const std::string& expected_hex) {
    if (expected_hex.size() != 64) return false;
    const std::string actual = ofFile(path);
    if (actual.size() != 64) return false;
    // Case-insensitive compare to forgive UPPER-case checksums from releases.
    for (std::size_t i = 0; i < 64; ++i) {
        const char a = static_cast<char>(std::tolower(static_cast<unsigned char>(actual[i])));
        const char b = static_cast<char>(std::tolower(static_cast<unsigned char>(expected_hex[i])));
        if (a != b) return false;
    }
    return true;
}

} // namespace ryazhenka::sha256

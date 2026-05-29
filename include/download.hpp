#pragma once

constexpr int ON = 1;
constexpr int OFF = 0;

#include <json.hpp>

namespace download {

    long downloadFile(const std::string& url, std::vector<std::uint8_t>& res, const std::string& output = "", int api = OFF);
    long downloadFile(const std::string& url, const std::string& output = "", int api = OFF);
    std::vector<std::pair<std::string, std::string>> getLinks(const std::string& url);
    std::vector<std::pair<std::string, std::string>> getLinksFromJson(const nlohmann::ordered_json& json_object);
    std::string fetchTitle(const std::string& url);
    long downloadPage(const std::string& url, std::string& res, const std::vector<std::string>& headers = {}, const std::string& body = "");
    long getRequest(const std::string& url, nlohmann::ordered_json& res, const std::vector<std::string>& headers = {}, const std::string& body = "");

    /// Resolve "@latest_asset:OWNER/REPO" into a concrete browser_download_url
    /// by querying api.github.com/repos/<slug>/releases/latest. Prefers the
    /// first asset whose name ends in ".zip" (the user-facing artefact);
    /// falls back to assets[0] if no .zip is present.
    /// Returns empty string on any failure (network, parse, no assets).
    std::string resolveLatestAssetUrl(const std::string& slug);

}  // namespace download
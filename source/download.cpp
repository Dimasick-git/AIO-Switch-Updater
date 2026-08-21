#include "download.hpp"

#include <curl/curl.h>
#include <math.h>
#include <mbedtls/base64.h>
#include <switch.h>
#include <time.h>

#include <algorithm>
#include <chrono>
#include <cstdint>
#include <filesystem>
#include <limits>
#include <regex>
#include <string>
#include <thread>

#include "fs.hpp"
#include "progress_event.hpp"
#include "ryazhenka_curl_retry.hpp"
#include "ryazhenka_logger.hpp"
#include "utils.hpp"

namespace i18n = brls::i18n;
using namespace i18n::literals;

constexpr const char API_AGENT[] = "HamletDuFromage";
constexpr int _1MiB = 0x100000;

using json = nlohmann::ordered_json;

namespace download {

    namespace {

        std::chrono::_V2::steady_clock::time_point time_old;
        double dlold;

        typedef struct
        {
            char* memory;
            size_t size;
        } MemoryStruct_t;

        typedef struct
        {
            u_int8_t* data;
            size_t data_size;
            u_int64_t offset;
            FILE* out;
            Aes128CtrContext* aes;
            bool write_failed;
            std::uint64_t received_bytes;
            // The release server can keep an HTTP connection open after every
            // Content-Length byte has arrived. Record that condition so the
            // progress callback can end the transfer instead of leaving the
            // UI forever at max-1 progress.
            bool body_complete;
        } ntwrk_struct_t;

        static bool flushOutput(ntwrk_struct_t& data)
        {
            if (data.offset == 0)
                return true;
            // downloadFile(..., output="") uses the fixed buffer only for
            // small API/text responses; refuse an oversized response rather
            // than writing past the buffer.
            if (!data.out)
                return false;
            if (fwrite(data.data, 1, static_cast<size_t>(data.offset), data.out) != data.offset) {
                data.write_failed = true;
                return false;
            }
            data.offset = 0;
            return true;
        }

        static size_t WriteMemoryCallback(void* contents, size_t size, size_t num_files, void* userp)
        {
            if (ProgressEvent::instance().getInterupt())
                return 0;

            ntwrk_struct_t* data_struct = static_cast<ntwrk_struct_t*>(userp);
            if (size != 0 && num_files > std::numeric_limits<size_t>::max() / size)
                return 0;
            const size_t realsize = size * num_files;

            // A one-mebibyte bounded buffer is used for SD writes. Flush before
            // adding a chunk that would exceed it and report any write failure
            // back to curl instead of silently keeping a truncated ZIP.
            if (realsize > data_struct->data_size - data_struct->offset) {
                if (!flushOutput(*data_struct))
                    return 0;
            }
            if (realsize > data_struct->data_size)
                return 0;

            if (data_struct->aes)
                aes128CtrCrypt(data_struct->aes, &data_struct->data[data_struct->offset], contents, realsize);
            else
                memcpy(&data_struct->data[data_struct->offset], contents, realsize);

            data_struct->offset += realsize;
            if (realsize > std::numeric_limits<std::uint64_t>::max() - data_struct->received_bytes) {
                data_struct->write_failed = true;
                return 0;
            }
            data_struct->received_bytes += static_cast<std::uint64_t>(realsize);
            if (data_struct->offset < data_struct->data_size)
                data_struct->data[data_struct->offset] = 0;
            return realsize;
        }

        int download_progress(void* p, double dltotal, double dlnow, double ultotal, double ulnow)
        {
            auto* state = static_cast<ntwrk_struct_t*>(p);
            if (ProgressEvent::instance().getInterupt())
                return 1;  // Abort cURL immediately when the user presses B.
            if (dltotal <= 0.0)
                return 0;

            const double fractionDownloaded = dlnow / dltotal;
            const int counter = static_cast<int>(fractionDownloaded * ProgressEvent::instance().getMax());
            ProgressEvent::instance().setStep(std::min(ProgressEvent::instance().getMax() - 1, counter));
            ProgressEvent::instance().setNow(dlnow);
            ProgressEvent::instance().setTotalCount(dltotal);
            const auto time_now = std::chrono::steady_clock::now();
            const double elapsed = std::chrono::duration<double>(time_now - time_old).count();
            if (elapsed > 1.2f) {
                ProgressEvent::instance().setSpeed((dlnow - dlold) / elapsed);
                dlold = dlnow;
                time_old = time_now;
            }

            // GitHub's CDN (or a captive/proxy network) may deliver the entire
            // declared body yet leave the connection open. All payload bytes
            // have already passed the write callback at this point. Stop cURL
            // deliberately; downloadFile accepts this expected callback abort,
            // flushes the final buffer and validates the exact file size.
            const auto expectedBytes = static_cast<std::uint64_t>(dltotal);
            if (state && expectedBytes > 0 && dlnow >= dltotal && state->received_bytes >= expectedBytes) {
                state->body_complete = true;
                return 1;
            }
            return 0;
        }

        struct MemoryStruct
        {
            char* memory;
            size_t size;
            size_t capacity;
        };

        static size_t WriteMemoryCallback2(void* contents, size_t size, size_t nmemb, void* userp)
        {
            size_t realsize = size * nmemb;
            struct MemoryStruct* mem = (struct MemoryStruct*)userp;

            // Grow exponentially to avoid O(n²) realloc chain for large responses.
            size_t needed = mem->size + realsize + 1;
            if (needed > mem->capacity) {
                size_t new_cap = mem->capacity ? mem->capacity * 2 : 4096;
                while (new_cap < needed) new_cap *= 2;
                char* ptr = static_cast<char*>(realloc(mem->memory, new_cap));
                if (!ptr) return 0;
                mem->memory = ptr;
                mem->capacity = new_cap;
            }

            memcpy(&(mem->memory[mem->size]), contents, realsize);
            mem->size += realsize;
            mem->memory[mem->size] = 0;

            return realsize;
        }

        bool checkSize(CURL* curl, const std::string& url)
        {
            curl_off_t dl;
            curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
            curl_easy_setopt(curl, CURLOPT_USERAGENT, API_AGENT);
            curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
            curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 0L);
            curl_easy_setopt(curl, CURLOPT_SSL_VERIFYHOST, 0L);
            curl_easy_setopt(curl, CURLOPT_NOBODY, 1L);
            curl_easy_setopt(curl, CURLOPT_CONNECTTIMEOUT, 5L);
            curl_easy_setopt(curl, CURLOPT_TIMEOUT, 15L);
            curl_easy_perform(curl);
            auto res = curl_easy_getinfo(curl, CURLINFO_CONTENT_LENGTH_DOWNLOAD_T, &dl);
            if (!res) {
                s64 freeStorage;
                if (R_SUCCEEDED(fs::getFreeStorageSD(freeStorage)) && dl * 2.5 > freeStorage) {
                    return false;
                }
            }
            return true;
        }

        std::string mega_id(std::string url)
        {
            auto len = url.length();

            if (len < 52)
                throw std::invalid_argument("Invalid URL.");

            bool old_link = url.find("#!") < len;

            /* Start from the last '/' and add 2 characters if old link format starting with '#!' */
            auto init_pos = url.find_last_of('/') + (old_link ? 2 : 0) + 1;

            /* End with the last '#' or "!" if its the old link format */
            auto end_pos = url.find_last_of(old_link ? '!' : '#');

            /* Finally crop the url */
            std::string id = url.substr(init_pos, end_pos - init_pos);

            if (id.length() != 8)
                throw std::invalid_argument("Invalid URL ID.");

            return id;
        }

        std::string mega_url(std::string url)
        {
            std::string id = mega_id(url);

            json request = json::array({{
                {"a", "g"},
                {"g", 1},
                {"p", id},
            }});

            std::string body = request.dump();
            std::string output;

            auto curl = curl_easy_init();
            curl_easy_setopt(curl, CURLOPT_URL, "https://g.api.mega.co.nz/cs");
            curl_easy_setopt(curl, CURLOPT_POST, 1L);
            curl_easy_setopt(curl, CURLOPT_POSTFIELDS, body.c_str());
            curl_easy_setopt(curl, CURLOPT_WRITEDATA, &output);
            curl_easy_setopt(curl, CURLOPT_USERAGENT, API_AGENT);
            curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
            curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 0L);
            curl_easy_setopt(curl, CURLOPT_SSL_VERIFYHOST, 0L);
            curl_easy_setopt(curl, CURLOPT_CONNECTTIMEOUT, 5L);
            curl_easy_setopt(curl, CURLOPT_TIMEOUT, 30L);
            curl_easy_setopt(
                curl,
                CURLOPT_WRITEFUNCTION,
                +[](void* buffer, size_t size, size_t nmemb, void* userp) -> size_t {
                    std::string* output = reinterpret_cast<std::string*>(userp);
                    size_t actual_size = size * nmemb;
                    output->append(reinterpret_cast<char*>(buffer), actual_size);
                    return actual_size;
                });

            curl_easy_perform(curl);

            json response = json::parse(output);

            curl_easy_cleanup(curl);

            s64 freeStorage;
            s64 fileSize = response[0]["s"];
            if (R_SUCCEEDED(fs::getFreeStorageSD(freeStorage)) && fileSize * 2.5 > freeStorage)
                return "";

            return response[0]["g"];
        }

        std::string mega_node_key(std::string url)
        {
            /* Check if old link format */
            auto len = url.length();

            if (len < 52)
                throw std::invalid_argument("Invalid URL.");

            bool old_link = url.find("#!") < len;

            /* End with the last '#' or "!" if its the old link format */
            auto end_pos = url.find_last_of(old_link ? '!' : '#') + 1;

            /* Crop the URL to get the file key */
            std::string key = url.substr(end_pos, len - end_pos);

            /* Replace URL characters with B64 characters */
            std::replace(key.begin(), key.end(), '_', '/');
            std::replace(key.begin(), key.end(), '-', '+');

            /* Add padding */
            auto key_len = key.length();
            unsigned pad = 4 - key_len % 4;
            key.append(pad, '=');

            /* The encoded key should have 44 characters to produce a 32 byte node key */
            if (key.length() != 44)
                throw std::invalid_argument("Invalid URL key.");

            std::string decoded(key.size() * 3 / 4, 0);
            size_t olen = 0;

            mbedtls_base64_decode(
                reinterpret_cast<unsigned char*>(decoded.data()),
                decoded.size(),
                &olen,
                reinterpret_cast<const unsigned char*>(key.c_str()),
                key.size());

            /**
             * The encoded base64 is (usually?) 43 characters long. With padding it goes
             * to 44. When we calculate the decoded size, we need to allocate 33 bytes.
             * But the last encoded character is padding, and combined with the last 
             * valid character, it should produce a 32 byte node key.
             */
            decoded.resize(olen);
            if (decoded.size() != 32)
                throw std::invalid_argument("Invalid node key.");

            return decoded;
        }

        std::string mega_key(const std::string& node_key)
        {
            std::string key(16, 0);

            reinterpret_cast<uint64_t*>(key.data())[0] =
                reinterpret_cast<const uint64_t*>(node_key.data())[0] ^
                reinterpret_cast<const uint64_t*>(node_key.data())[2];
            reinterpret_cast<uint64_t*>(key.data())[1] =
                reinterpret_cast<const uint64_t*>(node_key.data())[1] ^
                reinterpret_cast<const uint64_t*>(node_key.data())[3];

            return key;
        }

        std::string mega_iv(const std::string& node_key)
        {
            std::string iv(16, 0);
            reinterpret_cast<uint64_t*>(iv.data())[0] =
                reinterpret_cast<const uint64_t*>(node_key.data())[2];

            return iv;
        }
    }  // namespace

    long downloadFile(const std::string& url, const std::string& output, int api)
    {
        std::vector<std::uint8_t> dummy;
        return downloadFile(url, dummy, output, api);
    }

    long downloadFile(const std::string& url, std::vector<std::uint8_t>& res, const std::string& output, int api)
    {
        const char* out = output.c_str();
        CURL* curl = curl_easy_init();
        ntwrk_struct_t chunk = {0};
        long status_code = 0;
        time_old = std::chrono::steady_clock::now();
        dlold = 0.0f;
        bool can_download = true;
        bool is_mega = (url.find("mega.nz") != std::string::npos);
        std::string real_url = is_mega ? mega_url(url) : url;

        if (is_mega) {
            std::string node_key = mega_node_key(url);
            std::string key = mega_key(node_key);
            std::string iv = mega_iv(node_key);

            chunk.aes = static_cast<Aes128CtrContext*>(malloc(sizeof(Aes128CtrContext)));
            aes128CtrContextCreate(chunk.aes, key.c_str(), iv.c_str());
        }

        if (!curl) {
            status_code = 408;
        }
        else {
            FILE* fp = *out != 0 ? fopen(out, "wb") : nullptr;
            if (*out != 0 && !fp) {
                status_code = 507;
            }
            else {
                chunk.data = static_cast<u_int8_t*>(malloc(_1MiB));
                if (!chunk.data) {
                    status_code = 507;
                }
                else {
                    chunk.data_size = _1MiB;
                    chunk.out = fp;

                    if (*out != 0)
                        can_download = is_mega ? !real_url.empty() : checkSize(curl, url);

                    if (!can_download) {
                        // The preflight uses the remote compressed length and
                        // requires 2.5x room for the archive and its extraction.
                        status_code = 507;
                    }
                    else {
                        curl_easy_setopt(curl, CURLOPT_URL, real_url.c_str());
                        curl_easy_setopt(curl, CURLOPT_USERAGENT, API_AGENT);
                        curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
                        curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 0L);
                        curl_easy_setopt(curl, CURLOPT_SSL_VERIFYHOST, 0L);
                        curl_easy_setopt(curl, CURLOPT_NOBODY, 0L);
                        // checkSize() used this handle for HEAD and set a 15s
                        // timeout. Clear it for the real archive transfer.
                        curl_easy_setopt(curl, CURLOPT_TIMEOUT, 0L);
                        curl_easy_setopt(curl, CURLOPT_CONNECTTIMEOUT, 10L);
                        curl_easy_setopt(curl, CURLOPT_LOW_SPEED_LIMIT, 1L);
                        curl_easy_setopt(curl, CURLOPT_LOW_SPEED_TIME, 30L);
                        curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, WriteMemoryCallback);
                        curl_easy_setopt(curl, CURLOPT_WRITEDATA, &chunk);

                        if (api == OFF) {
                            curl_easy_setopt(curl, CURLOPT_NOPROGRESS, 0L);
                            curl_easy_setopt(curl, CURLOPT_PROGRESSFUNCTION, download_progress);
                            curl_easy_setopt(curl, CURLOPT_PROGRESSDATA, &chunk);
                        }

                        CURLcode cres = CURLE_OK;
                        for (int attempt = 0; attempt < 3; ++attempt) {
                            if (attempt > 0) {
                                if (fp) {
                                    fp = freopen(out, "wb", fp);
                                    chunk.out = fp;
                                }
                                chunk.offset = 0;
                                chunk.write_failed = false;
                                chunk.received_bytes = 0;
                                chunk.body_complete = false;
                                if (*out != 0 && !fp) {
                                    status_code = 507;
                                    break;
                                }
                            }

                            cres = curl_easy_perform(curl);
                            curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &status_code);
                            if (ProgressEvent::instance().getInterupt())
                                break;
                            // A callback abort after dlnow reached dltotal is a
                            // successful body transfer, not a network failure.
                            if (cres == CURLE_ABORTED_BY_CALLBACK && chunk.body_complete &&
                                status_code >= 200 && status_code < 300) {
                                cres = CURLE_OK;
                            }
                            if (chunk.write_failed) {
                                status_code = 507;
                                break;
                            }
                            if (cres == CURLE_OK && status_code < 500)
                                break;
                            if (attempt < 2 && ryazhenka::curl::isTransient(cres, status_code)) {
                                ryazhenka::log::warn(
                                    std::string("downloadFile: transient rc=") + std::to_string((int)cres) +
                                    " (" + curl_easy_strerror(cres) + ") http=" + std::to_string(status_code) + " — retrying");
                                std::this_thread::sleep_for(
                                    std::chrono::milliseconds(ryazhenka::kCurlBaseDelayMs * (attempt + 1)));
                                continue;
                            }
                            break;
                        }

                        if (cres != CURLE_OK && !chunk.write_failed && status_code < 400) {
                            ryazhenka::log::warn(
                                std::string("downloadFile: curl FAILED rc=") + std::to_string((int)cres) +
                                " (" + curl_easy_strerror(cres) + ") http=" + std::to_string(status_code) +
                                " url=" + real_url);
                            status_code = 408;
                        }

                        if (chunk.write_failed || !flushOutput(chunk) || (fp && fflush(fp) != 0)) {
                            status_code = 507;
                        }
                        else if (*out != 0 && status_code >= 200 && status_code < 300) {
                            curl_off_t expectedSize = -1;
                            curl_easy_getinfo(curl, CURLINFO_CONTENT_LENGTH_DOWNLOAD_T, &expectedSize);
                            std::error_code ec;
                            const auto written = std::filesystem::file_size(out, ec);
                            if (ec || (expectedSize >= 0 && written != static_cast<uintmax_t>(expectedSize))) {
                                ryazhenka::log::warn("downloadFile: downloaded size does not match Content-Length");
                                status_code = 409;
                            }
                        }
                    }
                }
            }

            curl_easy_cleanup(curl);
        }

        if (chunk.out) {
            if (fclose(chunk.out) != 0 && status_code < 400)
                status_code = 507;
            chunk.out = nullptr;
        }
        if (!can_download)
            res = {};

        if (*out == 0 && chunk.data)
            res.assign(chunk.data, chunk.data + chunk.offset);

        free(chunk.data);
        free(chunk.aes);

        return status_code;
    }

    std::string fetchTitle(const std::string& url)
    {
        CURL* curl_handle;
        struct MemoryStruct chunk;

        chunk.memory = static_cast<char*>(malloc(4096));
        chunk.size = 0;
        chunk.capacity = 4096;

        curl_handle = curl_easy_init();
        curl_easy_setopt(curl_handle, CURLOPT_URL, url);
        curl_easy_setopt(curl_handle, CURLOPT_WRITEFUNCTION, WriteMemoryCallback2);
        curl_easy_setopt(curl_handle, CURLOPT_WRITEDATA, (void*)&chunk);
        curl_easy_setopt(curl_handle, CURLOPT_USERAGENT, API_AGENT);
        curl_easy_setopt(curl_handle, CURLOPT_CONNECTTIMEOUT, 5L);
        curl_easy_setopt(curl_handle, CURLOPT_TIMEOUT, 15L);

        curl_easy_setopt(curl_handle, CURLOPT_SSL_VERIFYPEER, 0L);
        curl_easy_perform(curl_handle);

        /* check for errors */
        std::string ver = "-1";
        std::string s = std::string(chunk.memory);
        std::regex rgx("<title>.+</title>");
        std::smatch match;
        if (std::regex_search(s, match, rgx)) {
            //ver = std::stoi(match[0]);
            //std::cout << match[0].str().substr(match[0].str().find(" ") + 1, 6) << std::endl;
            ver = match[0].str().substr(match[0].str().find(" ") + 1, 5);
        }
        curl_easy_cleanup(curl_handle);
        free(chunk.memory);

        return ver;
    }

    long downloadPage(const std::string& url, std::string& res, const std::vector<std::string>& headers, const std::string& body)
    {
        CURL* curl_handle;
        struct MemoryStruct chunk;
        struct curl_slist* list = NULL;
        long status_code = 0;

        chunk.memory = static_cast<char*>(malloc(4096));
        chunk.size = 0;
        chunk.capacity = 4096;

        curl_handle = curl_easy_init();
        curl_easy_setopt(curl_handle, CURLOPT_URL, url.c_str());
        curl_easy_setopt(curl_handle, CURLOPT_CONNECTTIMEOUT, 5L);
        curl_easy_setopt(curl_handle, CURLOPT_TIMEOUT, 15L);
        if (!headers.empty()) {
            for (auto& h : headers) {
                list = curl_slist_append(list, h.c_str());
            }
            curl_easy_setopt(curl_handle, CURLOPT_HTTPHEADER, list);
        }
        if (body != "") {
            curl_easy_setopt(curl_handle, CURLOPT_POSTFIELDS, body.c_str());
        }

        curl_easy_setopt(curl_handle, CURLOPT_WRITEFUNCTION, WriteMemoryCallback2);
        curl_easy_setopt(curl_handle, CURLOPT_WRITEDATA, (void*)&chunk);
        curl_easy_setopt(curl_handle, CURLOPT_USERAGENT, API_AGENT);

        curl_easy_setopt(curl_handle, CURLOPT_SSL_VERIFYPEER, 0L);
        curl_easy_perform(curl_handle);
        curl_easy_getinfo(curl_handle, CURLINFO_RESPONSE_CODE, &status_code);
        curl_easy_cleanup(curl_handle);
        res = std::string(chunk.memory);
        free(chunk.memory);

        return status_code;
    }

    long getRequest(const std::string& url, nlohmann::ordered_json& res, const std::vector<std::string>& headers, const std::string& body)
    {
        std::string request;
        long status_code = downloadPage(url, request, headers, body);

        if (json::accept(request))
            res = nlohmann::ordered_json::parse(request);
        else
            res = nlohmann::ordered_json::object();

        return status_code;
    }

    std::vector<std::pair<std::string, std::string>> getLinks(const std::string& url)
    {
        nlohmann::ordered_json request;
        getRequest(url, request);

        std::vector<std::pair<std::string, std::string>> res;
        for (auto it = request.begin(); it != request.end(); ++it) {
            res.push_back(std::make_pair(it.key(), it.value()));
        }
        return res;
    }

    std::vector<std::pair<std::string, std::string>> getLinksFromJson(const nlohmann::ordered_json& json_object)
    {
        std::vector<std::pair<std::string, std::string>> res;
        for (auto it = json_object.begin(); it != json_object.end(); ++it) {
            res.push_back(std::make_pair(it.key(), it.value()));
        }
        return res;
    }

    // Picks the preferred download URL from a single release JSON node:
    // first .zip asset, falling back to assets[0]. Returns "" if neither.
    static std::string pickReleaseAssetUrl(const nlohmann::ordered_json& rel)
    {
        if (!rel.is_object() || !rel.contains("assets") || !rel["assets"].is_array())
            return {};
        std::string fallback;
        for (const auto& asset : rel["assets"]) {
            if (!asset.contains("browser_download_url") || !asset["browser_download_url"].is_string())
                continue;
            const std::string url = asset["browser_download_url"].get<std::string>();
            if (fallback.empty()) fallback = url;
            if (asset.contains("name") && asset["name"].is_string()) {
                const std::string name = asset["name"].get<std::string>();
                if (name.size() >= 4 && name.compare(name.size() - 4, 4, ".zip") == 0)
                    return url;
            }
        }
        return fallback;
    }

    std::vector<std::pair<std::string, std::string>> resolveAllReleases(const std::string& slug)
    {
        // GitHub /releases (no /latest) returns the freshest 30 by default; we
        // ask for 50 to give the user a long pick list. Same asset-selection
        // rules as resolveLatestAssetUrl — prefer .zip, fall back to assets[0].
        const std::string api_url = "https://api.github.com/repos/" + slug + "/releases?per_page=50";
        nlohmann::ordered_json payload;
        const long http = getRequest(api_url, payload);
        if (http != 200 || !payload.is_array()) return {};

        std::vector<std::pair<std::string, std::string>> out;
        out.reserve(payload.size());
        for (const auto& rel : payload) {
            if (!rel.is_object()) continue;
            if (!rel.contains("tag_name") || !rel["tag_name"].is_string()) continue;
            const std::string tag = rel["tag_name"].get<std::string>();
            const std::string url = pickReleaseAssetUrl(rel);
            if (!tag.empty() && !url.empty())
                out.emplace_back(tag, url);
        }
        return out;
    }

    std::string resolveLatestAssetUrl(const std::string& slug, const std::string& preferExt)
    {
        // GitHub /releases/latest returns the most recent non-draft,
        // non-prerelease release with its full asset list. We prefer the first
        // asset whose name ends in preferExt (".zip" for archives we unpack,
        // ".bin" for raw payloads); if none match we fall back to assets[0].
        // This covers ovlSysmodules / Mission-Control / NX_Firmware / hekate
        // where the asset filename embeds the version and breaks the
        // releases/latest/download/<filename> alias.
        const std::string api_url = "https://api.github.com/repos/" + slug + "/releases/latest";
        nlohmann::ordered_json payload;
        const long http = getRequest(api_url, payload);
        if (http != 200) return {};
        if (!payload.contains("assets") || !payload["assets"].is_array()) return {};

        const std::size_t extLen = preferExt.size();
        std::string fallback;
        for (const auto& asset : payload["assets"]) {
            if (!asset.contains("browser_download_url") || !asset["browser_download_url"].is_string()) continue;
            const std::string url = asset["browser_download_url"].get<std::string>();
            if (fallback.empty()) fallback = url;
            if (asset.contains("name") && asset["name"].is_string()) {
                const std::string name = asset["name"].get<std::string>();
                if (extLen > 0 && name.size() >= extLen &&
                    name.compare(name.size() - extLen, extLen, preferExt) == 0) {
                    return url;
                }
            }
        }
        return fallback;
    }

}  // namespace download
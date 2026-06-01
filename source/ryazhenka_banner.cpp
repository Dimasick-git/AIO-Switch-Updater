#include "ryazhenka_banner.hpp"

#include <algorithm>
#include <atomic>
#include <chrono>
#include <cstdio>
#include <filesystem>
#include <mutex>
#include <thread>
#include <utility>

#include <curl/curl.h>
#include <json.hpp>

#include "constants.hpp"
#include "download.hpp"
#include "fs.hpp"
#include "ryazhenka_config.hpp"
#include "ryazhenka_curl_retry.hpp"
#include "ryazhenka_logger.hpp"

namespace fs_ns = std::filesystem;

namespace ryazhenka::banner {

namespace {

std::atomic<bool> g_fetchInFlight{false};
// Tripped by signalShutdown() right before main() calls curl_global_cleanup.
// A running refresh thread checks this atomic between curl_easy_init's and
// gives up if the host is about to tear curl down under it — the previous
// startup-crash fix family (commit ca62519) required exactly this guarantee.
std::atomic<bool> g_shuttingDown{false};
std::mutex g_diskMutex;  // serialises writes to the cache file

std::size_t writeToString(void* ptr, std::size_t size, std::size_t nmemb, void* userdata) {
    auto* s = static_cast<std::string*>(userdata);
    s->append(static_cast<const char*>(ptr), size * nmemb);
    return size * nmemb;
}

// Best-effort: parse "tag_name" and "body" from the GitHub release feed and
// write each to its own small cache file next to the banner. Silent on
// failure — the banner refresh succeeded either way.
void fetchAndWritePackTagAndNotes() {
    if (g_shuttingDown.load()) return;
    CURL* curl = curl_easy_init();
    if (!curl)
        return;

    std::string body;
    body.reserve(4096);

    struct curl_slist* headers = nullptr;
    headers = curl_slist_append(headers, "User-Agent: RyazhenkaUpdater/1.0");
    headers = curl_slist_append(headers, "Accept: application/vnd.github+json");

    curl_easy_setopt(curl, CURLOPT_URL, kSigpatchesReleasesUrl);
    curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
    curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
    curl_easy_setopt(curl, CURLOPT_CONNECTTIMEOUT, kCurlConnectTimeoutSec);
    curl_easy_setopt(curl, CURLOPT_TIMEOUT, 20L);
    curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 0L);
    curl_easy_setopt(curl, CURLOPT_SSL_VERIFYHOST, 0L);
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, &writeToString);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &body);

    long http_code = 0;
    const CURLcode rc = curl::performWithRetry(curl, &http_code);
    curl_slist_free_all(headers);
    curl_easy_cleanup(curl);

    if (rc != CURLE_OK || http_code >= 400)
        return;

    try {
        auto json = nlohmann::json::parse(body, nullptr, /*allow_exceptions=*/false);
        if (!json.is_object())
            return;

        if (json.contains("tag_name") && json["tag_name"].is_string()) {
            const std::string tag = json["tag_name"].get<std::string>();
            if (!tag.empty()) {
                if (std::FILE* f = std::fopen(kPackTagCachePath, "w")) {
                    std::fwrite(tag.data(), 1, tag.size(), f);
                    std::fclose(f);
                    log::info(std::string("banner: cached pack tag ") + tag);
                }
            }
        }

        if (json.contains("body") && json["body"].is_string()) {
            const std::string notes = json["body"].get<std::string>();
            // Allow empty (some releases ship without notes) — just write
            // whatever GitHub gave us, capped to ~16 KiB so a stray giant
            // release body can't blow up the SD cache file.
            constexpr std::size_t kMaxNotes = 16 * 1024;
            const std::string clipped = notes.size() > kMaxNotes
                                            ? notes.substr(0, kMaxNotes)
                                            : notes;
            if (std::FILE* f = std::fopen(kPackNotesCachePath, "w")) {
                std::fwrite(clipped.data(), 1, clipped.size(), f);
                std::fclose(f);
            }
        }
    } catch (...) {
        // best effort
    }
}

}  // namespace

std::string cachedPath() {
    std::error_code ec;
    if (fs_ns::exists(kBannerCachePath, ec))
        return kBannerCachePath;
    return "";
}

bool cacheIsFresh() {
    std::error_code ec;
    if (!fs_ns::exists(kBannerCachePath, ec))
        return false;
    const auto mtime = fs_ns::last_write_time(kBannerCachePath, ec);
    if (ec)
        return false;
    const auto now = decltype(mtime)::clock::now();
    const auto ageHours =
        std::chrono::duration_cast<std::chrono::hours>(now - mtime).count();
    return ageHours < kBannerCacheTtlHours;
}

bool fetchNow() {
    if (g_shuttingDown.load()) return false;
    std::lock_guard<std::mutex> lk(g_diskMutex);
    if (g_shuttingDown.load()) return false;

    fs::createTree(CONFIG_PATH);

    // Use the upstream-tested download::downloadFile path instead of rolling
    // our own curl_easy_init — that earlier custom path was where the user
    // saw the banner silently not appear. downloadFile follows redirects,
    // honours the project's timeouts, and returns an HTTP status we can act on.
    const long http_code = download::downloadFile(kBannerUrl, kBannerCachePath, OFF);
    if (http_code != 200 && http_code != 0) {
        log::warn(std::string("banner: fetch failed http=") + std::to_string(http_code));
        std::error_code ec;
        fs_ns::remove(kBannerCachePath, ec);
        return false;
    }

    // Sanity-check the cached file: an empty file or one without a PNG / JPG
    // magic header means GitHub redirected us to an HTML 302 / 404 page that
    // got saved as the "image". Drop it so brls::Image doesn't hand us a
    // ghost icon next launch.
    std::error_code ec;
    const auto sz = fs_ns::file_size(kBannerCachePath, ec);
    if (ec || sz < 32) {
        log::warn("banner: cached file too small / unreadable; dropping");
        fs_ns::remove(kBannerCachePath, ec);
        return false;
    }
    {
        unsigned char magic[8] = {};
        if (std::FILE* mf = std::fopen(kBannerCachePath, "rb")) {
            std::fread(magic, 1, sizeof(magic), mf);
            std::fclose(mf);
        }
        const bool png  = magic[0]==0x89 && magic[1]==0x50 && magic[2]==0x4E && magic[3]==0x47;
        const bool jpg  = magic[0]==0xFF && magic[1]==0xD8 && magic[2]==0xFF;
        if (!png && !jpg) {
            log::warn("banner: cached file is not PNG/JPG; dropping");
            fs_ns::remove(kBannerCachePath, ec);
            return false;
        }
    }
    log::info("banner: cached fresh copy");

    // Same release, so refresh the cached pack tag + notes in the same pass —
    // one round-trip for everything the install/notes cards need.
    if (!g_shuttingDown.load())
        fetchAndWritePackTagAndNotes();
    return true;
}

bool refreshAsync() {
    if (g_shuttingDown.load()) return false;
    bool expected = false;
    if (!g_fetchInFlight.compare_exchange_strong(expected, true))
        return false;
    std::thread([] {
        try {
            fetchNow();
        } catch (...) {
            log::warn("banner: async fetch threw");
        }
        g_fetchInFlight.store(false);
    }).detach();
    return true;
}

void signalShutdown() {
    g_shuttingDown.store(true);
}

brls::Image* makeImage() {
    const std::string path = cachedPath();
    if (!cacheIsFresh())
        refreshAsync();
    if (path.empty())
        return nullptr;
    return new brls::Image(path);
}

brls::Image* makeCachedOnlyImage() {
    const std::string path = cachedPath();
    if (path.empty())
        return nullptr;
    return new brls::Image(path);
}

namespace {

// Hero-banner height in px. ~200 reads as a website hero strip on the 720p
// panel without pushing the list content too far down.
constexpr unsigned kBannerViewHeight = 200;

// Full-width hero banner. brls::Image's CROP mode is broken for wide views in
// this borealis fork (it overflows the layout) and SCALE distorts the artwork,
// so we draw the image ourselves with a CSS-`cover` fit: scale to fill the
// whole view, centre, and clip the vertical overflow to a rounded rect.
class BannerView : public brls::View {
  public:
    explicit BannerView(std::string path) : path_(std::move(path)) {
        this->setHeight(kBannerViewHeight);
    }

    ~BannerView() override { releaseTexture(); }

    // Texture is created lazily in draw() exactly once and kept for the life of
    // the view. We deliberately do NOT reload it on every willAppear — decoding
    // the ~1 MB banner JPEG on each tab switch was the visible hitch the user
    // felt entering Tools / About. brls::Image keeps its texture the same way.

    void draw(NVGcontext* vg, int x, int y, unsigned width, unsigned height,
              brls::Style* /*style*/, brls::FrameContext* /*ctx*/) override {
        if (texture_ == -1)
            reloadTexture();
        if (texture_ == -1 || imgW_ <= 0 || imgH_ <= 0)
            return;

        const float fx = static_cast<float>(x);
        const float fy = static_cast<float>(y);
        const float fw = static_cast<float>(width);
        const float fh = static_cast<float>(height);
        constexpr float kRadius = 12.0f;

        // cover: scale so the image fills both dimensions, keep aspect, centre.
        const float scale = std::max(fw / imgW_, fh / imgH_);
        const float dw = imgW_ * scale;
        const float dh = imgH_ * scale;
        const float ox = fx + (fw - dw) * 0.5f;
        const float oy = fy + (fh - dh) * 0.5f;

        NVGpaint paint = nvgImagePattern(vg, ox, oy, dw, dh, 0.0f, texture_, 1.0f);
        nvgBeginPath(vg);
        nvgRoundedRect(vg, fx, fy, fw, fh, kRadius);
        nvgFillPaint(vg, paint);
        nvgFill(vg);
    }

  private:
    void reloadTexture() {
        NVGcontext* vg = brls::Application::getNVGContext();
        if (!vg)
            return;
        releaseTexture();
        texture_ = nvgCreateImage(vg, path_.c_str(), 0);
        if (texture_ != -1)
            nvgImageSize(vg, texture_, &imgW_, &imgH_);
    }

    void releaseTexture() {
        if (texture_ != -1) {
            if (NVGcontext* vg = brls::Application::getNVGContext())
                nvgDeleteImage(vg, texture_);
            texture_ = -1;
            imgW_ = imgH_ = 0;
        }
    }

    std::string path_;
    int texture_ = -1;
    int imgW_ = 0, imgH_ = 0;
};

}  // namespace

brls::View* makeBannerView() {
    const std::string path = cachedPath();
    if (path.empty())
        return nullptr;
    return new BannerView(path);
}

namespace {

std::string slurp(const char* path) {
    std::error_code ec;
    if (!fs_ns::exists(path, ec))
        return "";
    std::FILE* f = std::fopen(path, "r");
    if (!f)
        return "";
    std::string out;
    char buf[4096];
    while (std::size_t n = std::fread(buf, 1, sizeof(buf), f))
        out.append(buf, n);
    std::fclose(f);
    return out;
}

}  // namespace

std::string cachedPackTag() {
    std::string out = slurp(kPackTagCachePath);
    // Strip stray whitespace / newlines for the small tag string.
    while (!out.empty() && (out.back() == '\n' || out.back() == '\r' || out.back() == ' '))
        out.pop_back();
    return out;
}

std::string cachedPackNotes() {
    return slurp(kPackNotesCachePath);
}

}  // namespace ryazhenka::banner

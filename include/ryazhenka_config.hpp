#pragma once

// Centralized configuration for the Ryazhenka CFW fork. All Ryazhenka-specific
// data lives here so daily upstream merges do not collide with our changes.
// Patched call-sites in upstream files reference identifiers from namespace
// ryazhenka:: defined below.

#include <cstddef>
#include <string_view>

namespace ryazhenka {

// Branding
inline constexpr std::string_view kAppTitle = "Ryazhenka Updater";
inline constexpr std::string_view kAppTitleLocalized = "\xD0\xA0\xD1\x8F\xD0\xB6\xD0\xB5\xD0\xBD\xD0\xBA\xD0\xB0 Updater";
inline constexpr std::string_view kAppAuthor = "Dimasick-git";
inline constexpr std::string_view kTelegramUrl = "https://t.me/Ryazhenkacfw";
inline constexpr std::string_view kHomepageUrl = "https://github.com/Dimasick-git/Ryzhenka";
inline constexpr std::string_view kRepoUrl = "https://github.com/Dimasick-git/AIO-Switch-Updater";

// Self-update endpoints
inline constexpr char kAppUrl[] =
    "https://github.com/Dimasick-git/AIO-Switch-Updater/releases/latest/download/Ryazhenka_AIO.zip";
inline constexpr char kTagsInfoUrl[] =
    "https://api.github.com/repos/Dimasick-git/AIO-Switch-Updater/releases/latest";
inline constexpr char kChangelogUrl[] =
    "https://github.com/Dimasick-git/AIO-Switch-Updater/releases";

// Source catalogue (Dimasick-git/nx-links mirrors the upstream nx-links schema)
inline constexpr char kNxLinksUrl[] =
    "https://raw.githubusercontent.com/Dimasick-git/nx-links/main/nx-links.json";
inline constexpr char kFirmwareUrl[] =
    "https://raw.githubusercontent.com/Dimasick-git/nx-links/main/firmwares.json";
inline constexpr char kCfwUrl[] =
    "https://raw.githubusercontent.com/Dimasick-git/nx-links/main/bootloaders.json";
inline constexpr char kAmsUrl[] =
    "https://raw.githubusercontent.com/Dimasick-git/nx-links/main/cfws.json";
inline constexpr char kSxosUrl[] =
    "https://raw.githubusercontent.com/Dimasick-git/nx-links/main/sxos.json";
inline constexpr char kHekateUrl[] =
    "https://raw.githubusercontent.com/Dimasick-git/nx-links/main/hekate.json";
inline constexpr char kPayloadUrl[] =
    "https://raw.githubusercontent.com/Dimasick-git/nx-links/main/payloads.json";
inline constexpr char kJcColorUrl[] =
    "https://raw.githubusercontent.com/Dimasick-git/AIO-Switch-Updater/main/jc_profiles.json";
inline constexpr char kPcColorUrl[] =
    "https://raw.githubusercontent.com/Dimasick-git/AIO-Switch-Updater/main/pc_profiles.json";

// Ryazhenka pack (full sd_card archive: Atmosphere + Hekate + sigpatches + tools)
inline constexpr char kPackUrl[] =
    "https://github.com/Dimasick-git/Ryzhenka/releases/latest/download/Ryazhenkabestcfw.zip";
inline constexpr char kPackFilename[] = "/config/aio-switch-updater/ryazhenka-pack.zip";

// Per-release banner image (bbootlogo.png on the Ryzhenka GitHub release).
// Downloaded lazily and cached to SD so we never block the UI on the network.
inline constexpr char kBannerUrl[] =
    "https://github.com/Dimasick-git/Ryzhenka/releases/latest/download/bbootlogo.png";
inline constexpr char kBannerCachePath[] = "/config/aio-switch-updater/banner.png";
// Latest pack release tag (e.g. "v7.2.1"), cached alongside the banner.
inline constexpr char kPackTagCachePath[] = "/config/aio-switch-updater/.pack_tag";
// How long the cached banner is considered fresh before a background re-fetch.
inline constexpr int kBannerCacheTtlHours = 6;

// Sigpatches staleness check — points at the Ryazhenka pack release feed
// because the pack bundles the freshest sigpatches we ship.
inline constexpr char kSigpatchesReleasesUrl[] =
    "https://api.github.com/repos/Dimasick-git/Ryzhenka/releases/latest";
inline constexpr char kSigpatchesRemoteCachePath[] =
    "/config/aio-switch-updater/.sigpatches_remote.json";
inline constexpr int  kSigpatchesStaleThresholdDays = 14;
inline constexpr int  kSigpatchesCacheTtlHours      = 6;

// Behaviour flags (kept here so they ship with a default even if config.json
// has not been touched yet).
inline constexpr bool kAutoHealthAfterActions = true;
inline constexpr int  kDashboardSampleHz      = 1;

// Localization
inline constexpr std::string_view kDefaultLanguage = "ru";

// curl retry policy (transient HTTP/DNS failures)
inline constexpr int kCurlMaxRetries = 4;
inline constexpr int kCurlBaseDelayMs = 500;
inline constexpr int kCurlMaxDelayMs = 8000;
inline constexpr long kCurlConnectTimeoutSec = 10;

// Logger
inline constexpr char kLogFilePath[] = "/config/aio-switch-updater/log.txt";
inline constexpr std::size_t kLogRotateBytes = 1u << 20;
inline constexpr int kLogRotateKeep = 3;
inline constexpr char kLogConfigKey[] = "verbose_log";

// Theme accents (matches the ryazhenka dairy colour palette)
inline constexpr char kThemeAccentLight[] = "#F5E6C8";
inline constexpr char kThemeAccentDark[] = "#8B3A2F";

// Ecosystem cross-links displayed in the About tab
struct EcosystemLink {
    std::string_view name;
    std::string_view url;
};

inline constexpr EcosystemLink kEcosystemLinks[] = {
    {"Ryazhenka pack",         "https://github.com/Dimasick-git/Ryzhenka"},
    {"Atmosphere",             "https://github.com/Dimasick-git/Atmosphere"},
    {"Sys-clk (RU)",           "https://github.com/Dimasick-git/Sys-clk"},
    {"Ryazhahand Overlay",     "https://github.com/Dimasick-git/Ryazhahand-Overlay"},
    {"FPSLocker",              "https://github.com/Dimasick-git/FPSLocker"},
    {"EdiZon",                 "https://github.com/Dimasick-git/EdiZon"},
    {"Fizeau",                 "https://github.com/Dimasick-git/Fizeau"},
    {"ovlSysmodules",          "https://github.com/Dimasick-git/ovlSysmodules"},
    {"Mission-Control",        "https://github.com/Dimasick-git/Mission-Control"},
    {"Ryazha Status Monitor",  "https://github.com/Dimasick-git/Ryazha-Status-Monitor"},
    {"Ryazhenka best CFW Tuner", "https://github.com/Dimasick-git/Ryazhenkabestcfw-Tuner"},
    {"Telegram",               "https://t.me/Ryazhenkacfw"},
};

} // namespace ryazhenka

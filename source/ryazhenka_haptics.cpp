#include "ryazhenka_haptics.hpp"

#include <algorithm>
#include <atomic>
#include <chrono>

#ifdef __SWITCH__
#include <switch.h>
#endif

#include "constants.hpp"
#include "fs.hpp"
#include "ryazhenka_logger.hpp"
#include "utils.hpp"

namespace ryazhenka::haptics {

namespace {

using clock = std::chrono::steady_clock;

// Two settings, mutated from the Settings screen (main/UI thread) and read by
// the per-frame scheduler (also main/UI thread). Atomics keep them honest even
// though every current caller is on the same thread.
// Defaults: rumble ON at 33% strength out of the box (per user request).
// init() still honours explicit haptics_enabled / haptics_strength values from
// config.json, so this only affects fresh configs.
std::atomic<bool> g_enabled{true};
std::atomic<float> g_strength{0.33f};

// --- Frame-driven scheduler state (touched ONLY from the main/UI thread) ---
//
// History: an earlier revision spawned a detached std::thread per buzz to send
// the stop value after a sleep. That model was the source of the "Program
// closed" crashes the user kept hitting in Settings / Status: every focus and
// every toggle fired a fresh thread that called libnx HID (hidSendVibration
// values) concurrently with the main input poll, and the thread churn under
// the tight applet heap was a reliable way to take the whole process down.
//
// borealis runs input handling, the click callbacks AND the per-frame
// background preFrame() (which calls tick()) all on a single thread, so we can
// drive the whole start → stop cycle from there with zero threads and zero
// cross-thread HID access. This is exactly what the header has always
// promised ("Frame-driven, non-blocking ... No worker threads").
bool g_active = false;
clock::time_point g_stopAt{};

// Optional single follow-up buzz (used by error() for its double pulse).
bool g_haveFollowup = false;
float g_followupIntensity = 0.0f;
int g_followupDuration = 0;
clock::time_point g_followupAt{};

#ifdef __SWITCH__
HidVibrationDeviceHandle g_handheld[2];
HidVibrationDeviceHandle g_dual[2];
HidVibrationDeviceHandle g_full[2];
bool g_initHandheld = false;
bool g_initDual = false;
bool g_initFull = false;

void sendAll(float amp) {
    HidVibrationValue v;
    v.amp_low   = amp;
    v.freq_low  = 160.0f;
    v.amp_high  = amp;
    v.freq_high = 320.0f;
    HidVibrationValue values[2] = {v, v};
    // Sending to a style that isn't currently connected is harmless.
    if (g_initHandheld)
        hidSendVibrationValues(g_handheld, values, 2);
    if (g_initDual)
        hidSendVibrationValues(g_dual, values, 2);
    if (g_initFull)
        hidSendVibrationValues(g_full, values, 2);
}
#else
void sendAll(float /*amp*/) {}
#endif

// Begins a buzz at the already-scaled amplitude `amp` for `duration_ms`. Runs
// on the main thread; the matching stop is delivered by tick().
void startBuzz(float amp, int duration_ms) {
    sendAll(amp);
    g_stopAt = clock::now() + std::chrono::milliseconds(std::max(1, duration_ms));
    g_active = true;
}

}  // namespace

void init() {
    try {
        nlohmann::ordered_json cfg = fs::parseJsonFile(CONFIG_FILE);
        if (cfg.is_object()) {
            if (cfg.contains("haptics_enabled") && cfg["haptics_enabled"].is_boolean())
                g_enabled.store(cfg["haptics_enabled"].get<bool>());
            if (cfg.contains("haptics_strength") && cfg["haptics_strength"].is_number())
                g_strength.store(std::clamp(cfg["haptics_strength"].get<float>(), 0.0f, 1.0f));
        }
    } catch (...) {
        // keep defaults
    }

#ifdef __SWITCH__
    if (util::isApplet()) {
        log::info("haptics: running as applet — skipping vibration init");
        return;
    }

    Result rc1 = hidInitializeVibrationDevices(g_handheld, 2, HidNpadIdType_Handheld,
                                               HidNpadStyleTag_NpadHandheld);
    g_initHandheld = R_SUCCEEDED(rc1);
    Result rc2 = hidInitializeVibrationDevices(g_dual, 2, HidNpadIdType_No1,
                                               HidNpadStyleTag_NpadJoyDual);
    g_initDual = R_SUCCEEDED(rc2);
    Result rc3 = hidInitializeVibrationDevices(g_full, 2, HidNpadIdType_No1,
                                               HidNpadStyleTag_NpadFullKey);
    g_initFull = R_SUCCEEDED(rc3);

    log::info(std::string("haptics: handheld=") + (g_initHandheld ? "ok" : "no") +
              " dual=" + (g_initDual ? "ok" : "no") +
              " full=" + (g_initFull ? "ok" : "no") +
              " enabled=" + (g_enabled.load() ? "yes" : "no"));
#else
    log::info("haptics: stub (non-switch build)");
#endif
}

void exit() {
    g_enabled.store(false);
    g_active = false;
    g_haveFollowup = false;
    sendAll(0.0f);
}

void tick() {
    const auto now = clock::now();

    // Stop an expired buzz.
    if (g_active && now >= g_stopAt) {
        sendAll(0.0f);
        g_active = false;
    }

    // Fire a queued follow-up once its delay elapses. pulse() re-applies the
    // strength scaling and the enabled check.
    if (g_haveFollowup && now >= g_followupAt) {
        g_haveFollowup = false;
        pulse(g_followupIntensity, g_followupDuration);
    }
}

void pulse(float intensity, int duration_ms) {
    if (!g_enabled.load())
        return;
    const float amp = std::clamp(intensity, 0.0f, 1.0f) * g_strength.load();
    startBuzz(amp, duration_ms);
}

void focus()   { pulse(0.30f, 30); }
void click()   { pulse(0.60f, 50); }
void success() { pulse(0.85f, 100); }

void error() {
    if (!g_enabled.load())
        return;
    pulse(0.55f, 50);
    // Schedule the second buzz on the frame scheduler — no thread needed.
    g_followupIntensity = 0.55f;
    g_followupDuration  = 40;
    g_followupAt        = clock::now() + std::chrono::milliseconds(110);
    g_haveFollowup      = true;
}

void setEnabled(bool enabled) {
    g_enabled.store(enabled);
    if (!enabled) {
        g_active = false;
        g_haveFollowup = false;
        sendAll(0.0f);
    }
}

bool isEnabled() { return g_enabled.load(); }

void setStrength(float strength) { g_strength.store(std::clamp(strength, 0.0f, 1.0f)); }

}  // namespace ryazhenka::haptics

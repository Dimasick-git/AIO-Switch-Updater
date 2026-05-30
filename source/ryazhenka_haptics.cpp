#include "ryazhenka_haptics.hpp"

#include <algorithm>
#include <atomic>
#include <chrono>
#include <thread>

#ifdef __SWITCH__
#include <switch.h>
#endif

#include "constants.hpp"
#include "fs.hpp"
#include "ryazhenka_logger.hpp"
#include "utils.hpp"

namespace ryazhenka::haptics {

namespace {

std::atomic<bool> g_enabled{true};
std::atomic<float> g_strength{1.0f};

// Each buzz spawns a tiny detached thread that handles its own
// start → sleep → zero-amp cycle. This was rewritten from a tick()-driven
// model because tick() needed the UI thread to keep running — and the user
// reported that pressing "Проверить сеть" (which synchronously blocks the
// UI on a sequence of curl_easy_perform calls for several seconds) made the
// rumble continue the whole time. A detached thread sleeps on its own kernel
// thread, so the buzz always ends on time regardless of what the UI is doing.
std::atomic<bool> g_buzzInFlight{false};

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

void runBuzz(float amp, int duration_ms) {
    bool expected = false;
    if (!g_buzzInFlight.compare_exchange_strong(expected, true))
        return;  // already buzzing — drop the new request rather than overlap
    std::thread([amp, duration_ms]() {
        sendAll(amp);
        std::this_thread::sleep_for(std::chrono::milliseconds(duration_ms));
        sendAll(0.0f);
        g_buzzInFlight.store(false);
    }).detach();
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
    g_enabled.store(false);  // worker threads will short-circuit before sending
    sendAll(0.0f);
}

// Frame-loop tick — no longer needed in the new model, but the symbol is
// still referenced by ryazhenka_background.cpp::preFrame(). Keep as a no-op
// so we don't need to regenerate that file just to drop the call.
void tick() {}

void pulse(float intensity, int duration_ms) {
    if (!g_enabled.load())
        return;
    const float amp = std::clamp(intensity, 0.0f, 1.0f) * g_strength.load();
    runBuzz(amp, duration_ms);
}

void focus()   { pulse(0.30f, 30); }
void click()   { pulse(0.60f, 50); }
void success() { pulse(0.85f, 100); }

void error() {
    pulse(0.55f, 50);
    // Schedule the second buzz on its own thread so we keep the "not blocking
    // the UI" guarantee even for compound patterns.
    std::thread([]() {
        std::this_thread::sleep_for(std::chrono::milliseconds(110));
        pulse(0.55f, 40);
    }).detach();
}

void setEnabled(bool enabled) {
    g_enabled.store(enabled);
    if (!enabled)
        sendAll(0.0f);
}

bool isEnabled() { return g_enabled.load(); }

void setStrength(float strength) { g_strength.store(std::clamp(strength, 0.0f, 1.0f)); }

}  // namespace ryazhenka::haptics

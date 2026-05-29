#include "ryazhenka_haptics.hpp"

#include <algorithm>
#include <chrono>

#ifdef __SWITCH__
#include <switch.h>
#endif

#include "constants.hpp"
#include "fs.hpp"
#include "ryazhenka_logger.hpp"

namespace ryazhenka::haptics {

namespace {

using clock = std::chrono::steady_clock;

bool g_enabled = true;
float g_strength = 1.0f;

bool g_active = false;
clock::time_point g_stopAt;

bool g_pending = false;
clock::time_point g_pendingAt;
float g_pendingAmp = 0.0f;
int g_pendingDur = 0;

#ifdef __SWITCH__
HidVibrationDeviceHandle g_handheld[2];
HidVibrationDeviceHandle g_dual[2];
bool g_initHandheld = false;
bool g_initDual = false;

void sendAll(float amp) {
    HidVibrationValue v;
    v.amp_low   = amp;
    v.freq_low  = 160.0f;
    v.amp_high  = amp;
    v.freq_high = 320.0f;
    HidVibrationValue values[2] = {v, v};
    if (g_initHandheld)
        hidSendVibrationValues(g_handheld, values, 2);
    if (g_initDual)
        hidSendVibrationValues(g_dual, values, 2);
}
#else
void sendAll(float /*amp*/) {}
#endif

void startPulse(float intensity, int duration_ms) {
    const float amp = std::clamp(intensity, 0.0f, 1.0f) * g_strength;
    sendAll(amp);
    g_active = true;
    g_stopAt = clock::now() + std::chrono::milliseconds(duration_ms);
}

}  // namespace

void init() {
    // Read user preferences (defaults: enabled, full strength).
    try {
        nlohmann::ordered_json cfg = fs::parseJsonFile(CONFIG_FILE);
        if (cfg.is_object()) {
            if (cfg.contains("haptics_enabled") && cfg["haptics_enabled"].is_boolean())
                g_enabled = cfg["haptics_enabled"].get<bool>();
            if (cfg.contains("haptics_strength") && cfg["haptics_strength"].is_number())
                g_strength = std::clamp(cfg["haptics_strength"].get<float>(), 0.0f, 1.0f);
        }
    } catch (...) {
        // keep defaults
    }

#ifdef __SWITCH__
    Result rc1 = hidInitializeVibrationDevices(g_handheld, 2, HidNpadIdType_Handheld,
                                               HidNpadStyleTag_NpadHandheld);
    g_initHandheld = R_SUCCEEDED(rc1);

    Result rc2 = hidInitializeVibrationDevices(g_dual, 2, HidNpadIdType_No1,
                                               HidNpadStyleTag_NpadJoyDual | HidNpadStyleTag_NpadFullKey);
    g_initDual = R_SUCCEEDED(rc2);

    log::info(std::string("haptics: handheld=") + (g_initHandheld ? "ok" : "no") +
              " dual=" + (g_initDual ? "ok" : "no") +
              " enabled=" + (g_enabled ? "yes" : "no"));
#else
    log::info("haptics: stub (non-switch build)");
#endif
}

void exit() {
    g_active = false;
    g_pending = false;
    sendAll(0.0f);
}

void tick() {
    if (!g_enabled)
        return;
    const auto now = clock::now();
    if (g_active && now >= g_stopAt) {
        sendAll(0.0f);
        g_active = false;
    }
    if (g_pending && now >= g_pendingAt) {
        g_pending = false;
        startPulse(g_pendingAmp, g_pendingDur);
    }
}

void pulse(float intensity, int duration_ms) {
    if (!g_enabled)
        return;
    startPulse(intensity, duration_ms);
}

void focus()   { pulse(0.30f, 30); }
void click()   { pulse(0.60f, 50); }
void success() { pulse(0.85f, 100); }

void error() {
    if (!g_enabled)
        return;
    startPulse(0.55f, 50);
    // Queue a second buzz after a short gap for a distinct "error" pattern.
    g_pending = true;
    g_pendingAt = clock::now() + std::chrono::milliseconds(110);
    g_pendingAmp = 0.55f;
    g_pendingDur = 40;
}

void setEnabled(bool enabled) {
    g_enabled = enabled;
    if (!enabled)
        exit();
}

bool isEnabled() { return g_enabled; }

void setStrength(float strength) { g_strength = std::clamp(strength, 0.0f, 1.0f); }

}  // namespace ryazhenka::haptics

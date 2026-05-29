#include "ryazhenka_audio.hpp"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <cstring>

#ifdef __SWITCH__
#include <switch.h>
#endif

#include "constants.hpp"
#include "fs.hpp"
#include "ryazhenka_logger.hpp"

namespace ryazhenka::audio {

namespace {

constexpr int kSampleRate = 48000;
constexpr int kChannels = 2;
constexpr std::size_t kAlignment = 0x1000;
// 0x5000 = 20480 bytes = ~107 ms of 48 kHz s16 stereo. Multiple of 0x1000 so
// it satisfies libnx audout's buffer-size alignment requirement.
constexpr std::size_t kBufferBytes = 0x5000;
constexpr std::size_t kSamplesPerBuffer =
    kBufferBytes / (sizeof(std::int16_t) * kChannels);

bool g_initialized = false;
bool g_enabled = false;

#ifdef __SWITCH__

struct Effect {
    AudioOutBuffer hdr[2];
    void* mem[2] = {nullptr, nullptr};
    int next = 0;
};

Effect g_focus, g_click, g_success, g_error;

// Single sine tone with an attack-decay envelope. Volume in [0, 1].
void fillSine(std::int16_t* dst, std::size_t samples, float freqHz, float volume) {
    constexpr float kTwoPi = 6.28318530718f;
    const float dt = 1.0f / static_cast<float>(kSampleRate);
    for (std::size_t i = 0; i < samples; ++i) {
        const float life = static_cast<float>(i) / static_cast<float>(samples);
        float env;
        if (life < 0.08f)       env = life / 0.08f;          // 8% attack
        else if (life > 0.35f)  env = std::max(0.0f, (1.0f - life) / 0.65f);  // 65% decay
        else                    env = 1.0f;
        const float s = std::sin(kTwoPi * freqHz * (i * dt)) * env * volume;
        const auto v = static_cast<std::int16_t>(s * 32760.0f);
        dst[i * 2 + 0] = v;
        dst[i * 2 + 1] = v;
    }
}

// Two-tone chord with the same envelope.
void fillChord(std::int16_t* dst, std::size_t samples,
               float f1, float f2, float volume) {
    constexpr float kTwoPi = 6.28318530718f;
    const float dt = 1.0f / static_cast<float>(kSampleRate);
    for (std::size_t i = 0; i < samples; ++i) {
        const float life = static_cast<float>(i) / static_cast<float>(samples);
        float env;
        if (life < 0.05f)       env = life / 0.05f;
        else if (life > 0.3f)   env = std::max(0.0f, (1.0f - life) / 0.7f);
        else                    env = 1.0f;
        const float t = i * dt;
        const float s = (std::sin(kTwoPi * f1 * t) + std::sin(kTwoPi * f2 * t))
                        * 0.5f * env * volume;
        const auto v = static_cast<std::int16_t>(s * 32760.0f);
        dst[i * 2 + 0] = v;
        dst[i * 2 + 1] = v;
    }
}

bool allocBuffersForEffect(Effect& e) {
    for (int i = 0; i < 2; ++i) {
        // C-style aligned_alloc — same call libultrahand uses for its DMA
        // playback buffer, so we know it's available in devkitPro's newlib.
        void* p = aligned_alloc(kAlignment, kBufferBytes);
        if (!p)
            return false;
        e.mem[i] = p;
        std::memset(p, 0, kBufferBytes);
        std::memset(&e.hdr[i], 0, sizeof(AudioOutBuffer));
        e.hdr[i].buffer      = p;
        e.hdr[i].buffer_size = kBufferBytes;
        e.hdr[i].data_size   = kBufferBytes;
    }
    return true;
}

void freeBuffersForEffect(Effect& e) {
    for (int i = 0; i < 2; ++i) {
        free(e.mem[i]);  // paired with aligned_alloc above
        e.mem[i] = nullptr;
    }
}

void playEffect(Effect& e) {
    if (!g_initialized || !g_enabled)
        return;
    // Round-robin between the two pre-baked buffers. If the chosen one is
    // still queued from a previous call, audoutAppendAudioOutBuffer just
    // returns an error code — we ignore it; the sound will play next time.
    (void)audoutAppendAudioOutBuffer(&e.hdr[e.next]);
    e.next = (e.next + 1) % 2;
}

#endif  // __SWITCH__

}  // namespace

void init() {
    // Read the user preference (off by default — UI sounds can surprise
    // someone who already has music playing in the system).
    try {
        nlohmann::ordered_json cfg = fs::parseJsonFile(CONFIG_FILE);
        if (cfg.is_object() && cfg.contains("ryazhenka_audio_enabled") &&
            cfg["ryazhenka_audio_enabled"].is_boolean())
            g_enabled = cfg["ryazhenka_audio_enabled"].get<bool>();
    } catch (...) {
        // keep default (off)
    }

#ifdef __SWITCH__
    Result rc = audoutInitialize();
    if (R_FAILED(rc)) {
        log::warn("audio: audoutInitialize failed");
        return;
    }
    rc = audoutStartAudioOut();
    if (R_FAILED(rc)) {
        log::warn("audio: audoutStartAudioOut failed");
        audoutExit();
        return;
    }

    bool ok = true;
    ok = ok && allocBuffersForEffect(g_focus);
    ok = ok && allocBuffersForEffect(g_click);
    ok = ok && allocBuffersForEffect(g_success);
    ok = ok && allocBuffersForEffect(g_error);
    if (!ok) {
        log::warn("audio: aligned_alloc failed");
        freeBuffersForEffect(g_focus);
        freeBuffersForEffect(g_click);
        freeBuffersForEffect(g_success);
        freeBuffersForEffect(g_error);
        audoutStopAudioOut();
        audoutExit();
        return;
    }

    // Render the four tones once.
    fillSine (static_cast<std::int16_t*>(g_focus.mem[0]),   kSamplesPerBuffer, 880.0f, 0.16f);
    std::memcpy(g_focus.mem[1],   g_focus.mem[0],   kBufferBytes);
    fillChord(static_cast<std::int16_t*>(g_click.mem[0]),   kSamplesPerBuffer, 660.0f, 880.0f, 0.20f);
    std::memcpy(g_click.mem[1],   g_click.mem[0],   kBufferBytes);
    fillChord(static_cast<std::int16_t*>(g_success.mem[0]), kSamplesPerBuffer, 880.0f, 1318.5f, 0.22f);
    std::memcpy(g_success.mem[1], g_success.mem[0], kBufferBytes);
    fillChord(static_cast<std::int16_t*>(g_error.mem[0]),   kSamplesPerBuffer, 220.0f, 311.0f, 0.20f);
    std::memcpy(g_error.mem[1],   g_error.mem[0],   kBufferBytes);

    g_initialized = true;
    log::info(std::string("audio: ready enabled=") + (g_enabled ? "yes" : "no"));
#else
    log::info("audio: stub (non-switch build)");
#endif
}

void exit() {
#ifdef __SWITCH__
    if (!g_initialized)
        return;
    audoutStopAudioOut();
    audoutExit();
    freeBuffersForEffect(g_focus);
    freeBuffersForEffect(g_click);
    freeBuffersForEffect(g_success);
    freeBuffersForEffect(g_error);
    g_initialized = false;
#endif
}

void focus()   {
#ifdef __SWITCH__
    playEffect(g_focus);
#endif
}
void click()   {
#ifdef __SWITCH__
    playEffect(g_click);
#endif
}
void success() {
#ifdef __SWITCH__
    playEffect(g_success);
#endif
}
void error()   {
#ifdef __SWITCH__
    playEffect(g_error);
#endif
}

void setEnabled(bool enabled) { g_enabled = enabled; }
bool isEnabled() { return g_enabled; }

}  // namespace ryazhenka::audio

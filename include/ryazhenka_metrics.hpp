#pragma once

/**
 * @file ryazhenka_metrics.hpp
 * @brief Thread-safe sampling layer for the live system dashboard.
 *
 * Owns a background worker that wakes once per second, queries the
 * battery (`psm`), thermal (`ts`) and filesystem services, and pushes
 * a single Snapshot into five ring buffers (one Series per metric).
 *
 * The UI side reads via copyInto() — read-side holds the mutex only for
 * the duration of a 120-float memcpy (~5 µs), never during draw().
 *
 * Safe to call from any thread; if a libnx service fails to initialise
 * (e.g. inside Ryujinx) the corresponding Series stays empty and the
 * chart renders a flat baseline with a "n/a" badge.
 *
 * @author Dimasick-git
 */

#include <array>
#include <atomic>
#include <cstddef>
#include <mutex>
#include <optional>
#include <string>
#include <thread>

namespace ryazhenka::metrics {

inline constexpr std::size_t kSeriesCapacity = 120;     // 2 minutes @ 1 Hz
inline constexpr int         kSampleIntervalMs = 1000;

enum class Metric {
    CpuTempC,
    SkinTempC,
    BatteryPct,
    SdFreePct,
    ProcessRamMb,
    COUNT,
};

/// Lock-protected ring buffer of floats.
class Series {
public:
    /// Push a fresh sample. Discards the oldest when full.
    void push(float v);

    /// Snapshot the buffer into `out` in chronological order (oldest first).
    /// Returns the number of valid samples written (0..kSeriesCapacity).
    std::size_t copyInto(std::array<float, kSeriesCapacity>& out) const;

    std::size_t size() const;
    bool empty() const;
    std::optional<float> latest() const;
    std::optional<float> min() const;
    std::optional<float> max() const;
    std::optional<float> avg() const;

private:
    mutable std::mutex mu_;
    std::array<float, kSeriesCapacity> buf_{};
    std::size_t head_ = 0;  // next write slot
    std::size_t size_ = 0;
};

/// Composite snapshot returned by Sampler::lastSnapshot().
struct Snapshot {
    float battery_pct   = 0.0f;
    float cpu_temp_c    = 0.0f;
    float skin_temp_c   = 0.0f;
    float sd_free_pct   = 0.0f;
    float process_ram_mb = 0.0f;
    bool  charging       = false;
    bool  battery_ok    = false;
    bool  thermal_ok    = false;
    bool  storage_ok    = false;
};

/// Process-wide background sampler. Lifetime tied to the StatusTab —
/// start() in onShow(), stop() in onHide() to save battery.
class Sampler {
public:
    static Sampler& instance();

    /// Idempotent: calling start() twice is a no-op until stop() is called.
    void start();

    /// Joins the worker thread (blocking, bounded by one sample interval).
    void stop();

    bool isRunning() const { return running_.load(); }

    /// Read-only access to per-metric history.
    const Series& seriesOf(Metric m) const;

    /// Last composite snapshot (cheap, mutex-protected copy).
    Snapshot lastSnapshot() const;

    ~Sampler();
    Sampler(const Sampler&)            = delete;
    Sampler& operator=(const Sampler&) = delete;

private:
    Sampler() = default;
    void workerLoop();
    void sampleOnce();
    void initServices();
    void deinitServices();

    std::array<Series, static_cast<std::size_t>(Metric::COUNT)> series_;
    mutable std::mutex snap_mu_;
    Snapshot last_snap_{};

    std::atomic<bool> running_{false};
    std::atomic<bool> stop_flag_{false};
    std::thread worker_;

    bool psm_inited_ = false;
    bool ts_inited_  = false;
};

/// Helper for chart unit labels.
const char* unitFor(Metric m);

/// Localized title key for chart header.
const char* i18nTitleKey(Metric m);

} // namespace ryazhenka::metrics

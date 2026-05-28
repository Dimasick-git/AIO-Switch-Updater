/**
 * @file ryazhenka_metrics.cpp
 * @brief See ryazhenka_metrics.hpp.
 * @author Dimasick-git
 */

#include "ryazhenka_metrics.hpp"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <filesystem>
#include <numeric>
#include <system_error>

#include <switch.h>

#include "ryazhenka_logger.hpp"

namespace ryazhenka::metrics {

// ── Series ────────────────────────────────────────────────────────────────

void Series::push(float v) {
    std::lock_guard<std::mutex> g(mu_);
    buf_[head_] = v;
    head_ = (head_ + 1) % kSeriesCapacity;
    if (size_ < kSeriesCapacity) ++size_;
}

std::size_t Series::copyInto(std::array<float, kSeriesCapacity>& out) const {
    std::lock_guard<std::mutex> g(mu_);
    if (size_ < kSeriesCapacity) {
        // Buffer not full yet: oldest sample at index 0, newest at head_-1.
        for (std::size_t i = 0; i < size_; ++i) {
            out[i] = buf_[i];
        }
    } else {
        // Buffer full: oldest sample at head_, newest at head_-1 (wraparound).
        for (std::size_t i = 0; i < kSeriesCapacity; ++i) {
            out[i] = buf_[(head_ + i) % kSeriesCapacity];
        }
    }
    return size_;
}

std::size_t Series::size() const {
    std::lock_guard<std::mutex> g(mu_);
    return size_;
}

bool Series::empty() const {
    return size() == 0;
}

std::optional<float> Series::latest() const {
    std::lock_guard<std::mutex> g(mu_);
    if (size_ == 0) return std::nullopt;
    const std::size_t idx = (head_ + kSeriesCapacity - 1) % kSeriesCapacity;
    return buf_[idx];
}

std::optional<float> Series::min() const {
    std::lock_guard<std::mutex> g(mu_);
    if (size_ == 0) return std::nullopt;
    float m = buf_[0];
    if (size_ < kSeriesCapacity) {
        for (std::size_t i = 1; i < size_; ++i) m = std::min(m, buf_[i]);
    } else {
        for (std::size_t i = 0; i < kSeriesCapacity; ++i) m = std::min(m, buf_[i]);
    }
    return m;
}

std::optional<float> Series::max() const {
    std::lock_guard<std::mutex> g(mu_);
    if (size_ == 0) return std::nullopt;
    float m = buf_[0];
    if (size_ < kSeriesCapacity) {
        for (std::size_t i = 1; i < size_; ++i) m = std::max(m, buf_[i]);
    } else {
        for (std::size_t i = 0; i < kSeriesCapacity; ++i) m = std::max(m, buf_[i]);
    }
    return m;
}

std::optional<float> Series::avg() const {
    std::lock_guard<std::mutex> g(mu_);
    if (size_ == 0) return std::nullopt;
    double sum = 0.0;
    const std::size_t n = (size_ < kSeriesCapacity) ? size_ : kSeriesCapacity;
    for (std::size_t i = 0; i < n; ++i) sum += buf_[i];
    return static_cast<float>(sum / static_cast<double>(n));
}

// ── Sampler ───────────────────────────────────────────────────────────────

Sampler& Sampler::instance() {
    static Sampler s;
    return s;
}

Sampler::~Sampler() {
    stop();
}

void Sampler::start() {
    if (running_.exchange(true)) return;     // already running
    stop_flag_.store(false);
    worker_ = std::thread([this] { workerLoop(); });
    log::info("metrics: sampler started");
}

void Sampler::stop() {
    if (!running_.exchange(false)) return;
    stop_flag_.store(true);
    if (worker_.joinable()) worker_.join();
    deinitServices();
    log::info("metrics: sampler stopped");
}

const Series& Sampler::seriesOf(Metric m) const {
    return series_[static_cast<std::size_t>(m)];
}

Snapshot Sampler::lastSnapshot() const {
    std::lock_guard<std::mutex> g(snap_mu_);
    return last_snap_;
}

void Sampler::initServices() {
#ifdef __SWITCH__
    if (!psm_inited_) {
        if (R_SUCCEEDED(psmInitialize())) {
            psm_inited_ = true;
        } else {
            log::warn("metrics: psmInitialize failed");
        }
    }
    if (!ts_inited_) {
        if (R_SUCCEEDED(tsInitialize())) {
            ts_inited_ = true;
        } else {
            log::warn("metrics: tsInitialize failed");
        }
    }
#endif
}

void Sampler::deinitServices() {
#ifdef __SWITCH__
    if (psm_inited_) { psmExit(); psm_inited_ = false; }
    if (ts_inited_)  { tsExit();  ts_inited_  = false; }
#endif
}

void Sampler::sampleOnce() {
    Snapshot snap;
#ifdef __SWITCH__
    if (psm_inited_) {
        std::uint32_t pct = 0;
        if (R_SUCCEEDED(psmGetBatteryChargePercentage(&pct))) {
            snap.battery_pct = static_cast<float>(pct);
            snap.battery_ok  = true;
        }
        PsmChargerType type = PsmChargerType_Unconnected;
        if (R_SUCCEEDED(psmGetChargerType(&type))) {
            snap.charging = (type != PsmChargerType_Unconnected);
        }
    }
    if (ts_inited_) {
        std::int32_t milli = 0;
        if (R_SUCCEEDED(tsGetTemperatureMilliC(TsLocation_Internal, &milli))) {
            snap.cpu_temp_c = static_cast<float>(milli) / 1000.0f;
            snap.thermal_ok = true;
        }
        if (R_SUCCEEDED(tsGetTemperatureMilliC(TsLocation_External, &milli))) {
            snap.skin_temp_c = static_cast<float>(milli) / 1000.0f;
        }
    }
#endif
    // SD free percentage (works on Linux too, useful for dev builds)
    {
        std::error_code ec;
        const auto sp = std::filesystem::space("/", ec);
        if (!ec && sp.capacity > 0) {
            snap.sd_free_pct = static_cast<float>(
                100.0 * static_cast<double>(sp.available) /
                static_cast<double>(sp.capacity));
            snap.storage_ok = true;
        }
    }
    // Process RAM (approx) — svcGetInfo is libnx-only; on host return 0.
#ifdef __SWITCH__
    {
        std::uint64_t used = 0;
        if (R_SUCCEEDED(svcGetInfo(&used, InfoType_UsedMemorySize, CUR_PROCESS_HANDLE, 0))) {
            snap.process_ram_mb = static_cast<float>(used) / (1024.0f * 1024.0f);
        }
    }
#endif

    // Push into series (skip metrics whose service init failed — keeps the
    // chart visually honest: a flat empty area instead of a fake 0 line).
    if (snap.battery_ok)  series_[static_cast<std::size_t>(Metric::BatteryPct)].push(snap.battery_pct);
    if (snap.thermal_ok)  series_[static_cast<std::size_t>(Metric::CpuTempC)].push(snap.cpu_temp_c);
    if (snap.thermal_ok)  series_[static_cast<std::size_t>(Metric::SkinTempC)].push(snap.skin_temp_c);
    if (snap.storage_ok)  series_[static_cast<std::size_t>(Metric::SdFreePct)].push(snap.sd_free_pct);
    if (snap.process_ram_mb > 0.0f) {
        series_[static_cast<std::size_t>(Metric::ProcessRamMb)].push(snap.process_ram_mb);
    }

    {
        std::lock_guard<std::mutex> g(snap_mu_);
        last_snap_ = snap;
    }
}

void Sampler::workerLoop() {
    initServices();
    while (!stop_flag_.load()) {
        try {
            sampleOnce();
        } catch (const std::exception& e) {
            log::warn(std::string("metrics: sample exception — ") + e.what());
        } catch (...) {
            log::warn("metrics: unknown sample exception");
        }
        // Sleep in small slices so stop() returns quickly.
        for (int i = 0; i < 10 && !stop_flag_.load(); ++i) {
            std::this_thread::sleep_for(
                std::chrono::milliseconds(kSampleIntervalMs / 10));
        }
    }
}

// ── helpers ───────────────────────────────────────────────────────────────

const char* unitFor(Metric m) {
    switch (m) {
        case Metric::CpuTempC:    return "°C";
        case Metric::SkinTempC:   return "°C";
        case Metric::BatteryPct:  return "%";
        case Metric::SdFreePct:   return "%";
        case Metric::ProcessRamMb:return "MB";
        default:                  return "";
    }
}

const char* i18nTitleKey(Metric m) {
    switch (m) {
        case Metric::CpuTempC:    return "menus/ryazhenka/chart_cpu_temp";
        case Metric::SkinTempC:   return "menus/ryazhenka/chart_skin_temp";
        case Metric::BatteryPct:  return "menus/ryazhenka/chart_battery";
        case Metric::SdFreePct:   return "menus/ryazhenka/chart_sd_free";
        case Metric::ProcessRamMb:return "menus/ryazhenka/chart_process_ram";
        default:                  return "";
    }
}

} // namespace ryazhenka::metrics

/**
 * @file ryazhenka_chart_view.cpp
 * @brief See ryazhenka_chart_view.hpp.
 * @author Dimasick-git
 */

#include "ryazhenka_chart_view.hpp"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <limits>

namespace ryazhenka::chart {

namespace {

std::string defaultFormat(float v, const std::string& unit) {
    char buf[32];
    std::snprintf(buf, sizeof(buf), "%.1f%s", v, unit.c_str());
    return std::string(buf);
}

} // anonymous

ChartView::ChartView(ChartConfig cfg, SeriesProvider provider)
    : cfg_(std::move(cfg))
    , provider_(std::move(provider))
{
    if (!cfg_.formatter) {
        const std::string unit = cfg_.unit;
        cfg_.formatter = [unit](float v) { return defaultFormat(v, unit); };
    }
}

void ChartView::layout(NVGcontext* /*vg*/, brls::Style* /*style*/,
                       brls::FontStash* /*stash*/) {
    // Nothing to compute — we honour whatever box the parent gave us.
}

void ChartView::frame(brls::FrameContext* ctx) {
    // Pull a fresh window from the data source ~10 Hz; in between, brls
    // still calls frame() so background effects keep ticking but the
    // sample buffer is only refreshed periodically.
    if ((repaint_counter_++ % 6) == 0) {
        if (provider_) {
            valid_samples_ = provider_(sample_buf_);
        }
        this->invalidate();
    }
    View::frame(ctx);
}

void ChartView::draw(NVGcontext* vg, int x, int y, unsigned width,
                     unsigned height, brls::Style* /*style*/,
                     brls::FrameContext* ctx) {
    // ── Panel background ────────────────────────────────────────────
    nvgBeginPath(vg);
    nvgRoundedRect(vg, x + 2, y + 2, width - 4, height - 4, 8.0f);
    nvgFillColor(vg, nvgRGBAf(0.09f, 0.09f, 0.12f, 0.70f));
    nvgFill(vg);

    nvgBeginPath(vg);
    nvgRoundedRect(vg, x + 2, y + 2, width - 4, height - 4, 8.0f);
    nvgStrokeColor(vg, nvgRGBAf(1.0f, 1.0f, 1.0f, 0.10f));
    nvgStrokeWidth(vg, 1.5f);
    nvgStroke(vg);

    const int padX  = 16;
    const int padTop = 12;
    const int padBot = 30;

    const int headerY = y + padTop;
    const int plotX = x + padX;
    const int plotY = headerY + 28;
    const int plotW = static_cast<int>(width) - padX * 2;
    const int plotH = static_cast<int>(height) - (padTop + 28 + padBot);

    if (plotW <= 0 || plotH <= 0) return;

    // ── Header: title + current value ──────────────────────────────
    nvgFontFaceId(vg, ctx->fontStash->regular);
    nvgFillColor(vg, nvgRGBAf(0.95f, 0.95f, 0.95f, 1.0f));
    nvgFontSize(vg, 20.0f);
    nvgTextAlign(vg, NVG_ALIGN_LEFT | NVG_ALIGN_TOP);
    nvgText(vg, x + padX, headerY, cfg_.title.c_str(), nullptr);

    const float current = (valid_samples_ > 0)
        ? sample_buf_[valid_samples_ - 1]
        : std::numeric_limits<float>::quiet_NaN();
    const std::string currentStr =
        std::isnan(current) ? std::string("n/a")
                            : cfg_.formatter(current);
    nvgFontSize(vg, 26.0f);
    nvgFillColor(vg, cfg_.stroke_color);
    nvgTextAlign(vg, NVG_ALIGN_RIGHT | NVG_ALIGN_TOP);
    nvgText(vg, x + width - padX, headerY - 2, currentStr.c_str(), nullptr);

    // ── Grid: 4 horizontal lines + 25/50/75/100 % markers ──────────
    nvgStrokeWidth(vg, 1.0f);
    nvgStrokeColor(vg, nvgRGBAf(1.0f, 1.0f, 1.0f, 0.08f));
    for (int i = 0; i <= 4; ++i) {
        const float ly = plotY + (plotH * i) / 4.0f;
        nvgBeginPath(vg);
        nvgMoveTo(vg, plotX, ly);
        nvgLineTo(vg, plotX + plotW, ly);
        nvgStroke(vg);
    }

    // ── Sample polyline ────────────────────────────────────────────
    const float range = std::max(0.001f, cfg_.max_y - cfg_.min_y);
    auto valueToY = [&](float v) {
        const float t = std::clamp((v - cfg_.min_y) / range, 0.0f, 1.0f);
        return plotY + plotH - (t * plotH);
    };

    if (valid_samples_ >= 2) {
        const std::size_t n =
            std::min(valid_samples_, ryazhenka::metrics::kSeriesCapacity);
        const float stepX =
            static_cast<float>(plotW) / static_cast<float>(n - 1);

        // Area fill underneath the polyline.
        nvgBeginPath(vg);
        nvgMoveTo(vg, plotX, plotY + plotH);
        for (std::size_t i = 0; i < n; ++i) {
            const float px = plotX + stepX * static_cast<float>(i);
            const float py = valueToY(sample_buf_[i]);
            nvgLineTo(vg, px, py);
        }
        nvgLineTo(vg, plotX + plotW, plotY + plotH);
        nvgClosePath(vg);
        nvgFillColor(vg, cfg_.fill_color);
        nvgFill(vg);

        // Line itself.
        nvgBeginPath(vg);
        nvgMoveTo(vg, plotX, valueToY(sample_buf_[0]));
        for (std::size_t i = 1; i < n; ++i) {
            const float px = plotX + stepX * static_cast<float>(i);
            const float py = valueToY(sample_buf_[i]);
            nvgLineTo(vg, px, py);
        }
        nvgStrokeColor(vg, cfg_.stroke_color);
        nvgStrokeWidth(vg, 2.5f);
        nvgLineCap(vg, NVG_ROUND);
        nvgLineJoin(vg, NVG_ROUND);
        nvgStroke(vg);

        // Highlight the latest point with a small filled circle.
        const float lastX = plotX + plotW;
        const float lastY = valueToY(sample_buf_[n - 1]);
        nvgBeginPath(vg);
        nvgCircle(vg, lastX, lastY, 4.0f);
        nvgFillColor(vg, cfg_.stroke_color);
        nvgFill(vg);
    } else {
        nvgFontSize(vg, 18.0f);
        nvgFontFaceId(vg, ctx->fontStash->regular);
        nvgFillColor(vg, nvgRGBAf(1.0f, 1.0f, 1.0f, 0.45f));
        nvgTextAlign(vg, NVG_ALIGN_CENTER | NVG_ALIGN_MIDDLE);
        nvgText(vg, plotX + plotW / 2, plotY + plotH / 2,
                "ожидание данных…", nullptr);
    }

    // ── Footer: min / avg / max ─────────────────────────────────────
    if (valid_samples_ > 0) {
        float mn = sample_buf_[0];
        float mx = sample_buf_[0];
        double sum = 0.0;
        const std::size_t n =
            std::min(valid_samples_, ryazhenka::metrics::kSeriesCapacity);
        for (std::size_t i = 0; i < n; ++i) {
            mn = std::min(mn, sample_buf_[i]);
            mx = std::max(mx, sample_buf_[i]);
            sum += sample_buf_[i];
        }
        const float av = static_cast<float>(sum / static_cast<double>(n));

        char buf[160];
        std::snprintf(buf, sizeof(buf),
                      "мин %.1f%s   ср %.1f%s   макс %.1f%s",
                      mn, cfg_.unit.c_str(),
                      av, cfg_.unit.c_str(),
                      mx, cfg_.unit.c_str());
        nvgFontSize(vg, 16.0f);
        nvgFillColor(vg, nvgRGBAf(1.0f, 1.0f, 1.0f, 0.70f));
        nvgTextAlign(vg, NVG_ALIGN_LEFT | NVG_ALIGN_BOTTOM);
        nvgText(vg, x + padX, y + height - 10, buf, nullptr);
    }
}

// ── Factory ──────────────────────────────────────────────────────────

namespace {

ChartConfig configFor(ryazhenka::metrics::Metric m) {
    using ryazhenka::metrics::Metric;
    ChartConfig c;
    c.unit = ryazhenka::metrics::unitFor(m);
    switch (m) {
        case Metric::CpuTempC:
            c.title  = "CPU";
            c.min_y  = 20.0f; c.max_y = 85.0f;
            c.stroke_color = nvgRGBAf(0.95f, 0.45f, 0.20f, 1.0f);   // orange
            c.fill_color   = nvgRGBAf(0.95f, 0.45f, 0.20f, 0.22f);
            break;
        case Metric::SkinTempC:
            c.title  = "Корпус";
            c.min_y  = 15.0f; c.max_y = 55.0f;
            c.stroke_color = nvgRGBAf(0.92f, 0.78f, 0.20f, 1.0f);   // amber
            c.fill_color   = nvgRGBAf(0.92f, 0.78f, 0.20f, 0.22f);
            break;
        case Metric::BatteryPct:
            c.title  = "Аккумулятор";
            c.min_y  = 0.0f; c.max_y = 100.0f;
            c.stroke_color = nvgRGBAf(0.30f, 0.85f, 0.45f, 1.0f);   // green
            c.fill_color   = nvgRGBAf(0.30f, 0.85f, 0.45f, 0.22f);
            break;
        case Metric::SdFreePct:
            c.title  = "Свободно SD";
            c.min_y  = 0.0f; c.max_y = 100.0f;
            c.stroke_color = nvgRGBAf(0.35f, 0.65f, 1.00f, 1.0f);   // blue
            c.fill_color   = nvgRGBAf(0.35f, 0.65f, 1.00f, 0.22f);
            break;
        case Metric::ProcessRamMb:
            c.title  = "RAM процесса";
            c.min_y  = 0.0f; c.max_y = 512.0f;
            c.stroke_color = nvgRGBAf(0.75f, 0.55f, 0.95f, 1.0f);   // violet
            c.fill_color   = nvgRGBAf(0.75f, 0.55f, 0.95f, 0.22f);
            break;
        default:
            c.title = "?";
    }
    return c;
}

} // anonymous

ChartView* makeChartFor(ryazhenka::metrics::Metric metric) {
    auto cfg = configFor(metric);
    auto provider = [metric](std::array<float,
                                        ryazhenka::metrics::kSeriesCapacity>& out)
                    -> std::size_t {
        return ryazhenka::metrics::Sampler::instance()
                   .seriesOf(metric).copyInto(out);
    };
    return new ChartView(std::move(cfg), std::move(provider));
}

} // namespace ryazhenka::chart

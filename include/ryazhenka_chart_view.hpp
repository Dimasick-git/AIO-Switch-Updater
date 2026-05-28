#pragma once

/**
 * @file ryazhenka_chart_view.hpp
 * @brief Live line-chart widget drawn directly with NanoVG.
 *
 * Subclass of brls::View. Renders a 120-sample time series as a
 * polyline with a translucent area fill underneath, plus a header
 * (title + current value) and a footer (min / avg / max).
 *
 * The View::frame() override repaints at ~10 Hz (every 6th frame at
 * 60 fps) which is plenty for second-resolution sources and keeps GPU
 * load low.
 *
 * Data source is supplied as a std::function so the widget can be
 * tested off-device with a synthetic generator.
 *
 * @author Dimasick-git
 */

#include <array>
#include <cstddef>
#include <functional>
#include <string>

#include <borealis.hpp>
#include <nanovg.h>

#include "ryazhenka_metrics.hpp"

namespace ryazhenka::chart {

struct ChartConfig {
    std::string title;
    std::string unit;
    float min_y = 0.0f;
    float max_y = 100.0f;
    NVGcolor stroke_color{};   // configured in configFor()
    NVGcolor fill_color{};
    // value -> printable string. nullptr → default "%.1f<unit>".
    std::function<std::string(float)> formatter = nullptr;
};

using SeriesProvider = std::function<std::size_t(
    std::array<float, ryazhenka::metrics::kSeriesCapacity>&)>;

class ChartView : public brls::View {
public:
    ChartView(ChartConfig cfg, SeriesProvider provider);

    void draw(NVGcontext* vg, int x, int y, unsigned width, unsigned height,
              brls::Style* style, brls::FrameContext* ctx) override;
    void layout(NVGcontext* vg, brls::Style* style, brls::FontStash* stash) override;
    void frame(brls::FrameContext* ctx) override;

private:
    ChartConfig cfg_;
    SeriesProvider provider_;
    std::array<float, ryazhenka::metrics::kSeriesCapacity> sample_buf_{};
    std::size_t valid_samples_ = 0;
    int repaint_counter_ = 0;
};

/// Factory: returns a configured ChartView wired to the global Sampler.
ChartView* makeChartFor(ryazhenka::metrics::Metric metric);

} // namespace ryazhenka::chart

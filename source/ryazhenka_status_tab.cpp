/**
 * @file ryazhenka_status_tab.cpp
 * @brief See ryazhenka_status_tab.hpp.
 * @author Dimasick-git
 */

#include "ryazhenka_status_tab.hpp"

#include <string>

#include "ryazhenka_chart_view.hpp"
#include "ryazhenka_logger.hpp"
#include "ryazhenka_metrics.hpp"
#include "ryazhenka_system_info.hpp"

namespace i18n = brls::i18n;
using namespace i18n::literals;

namespace ryazhenka {

namespace {

brls::BoxLayout* makeChartRow(brls::View* left, brls::View* right) {
    auto* row = new brls::BoxLayout(brls::BoxLayoutOrientation::HORIZONTAL);
    row->setSpacing(12);
    row->addView(left,  true /* fill */);
    row->addView(right, true);
    return row;
}

} // anonymous

StatusTab::StatusTab()
    : brls::BoxLayout(brls::BoxLayoutOrientation::VERTICAL)
{
    this->setSpacing(12);
    this->setMargins(16, 16, 16, 16);

    std::string header = "menus/ryazhenka/status_header"_i18n;
    try {
        const auto snap = sysinfo::collect();
        header += "  —  " + sysinfo::formatOneLine(snap);
    } catch (...) {
        header += "  —  (sysinfo unavailable)";
    }
    auto* lbl = new brls::Label(brls::LabelStyle::REGULAR, header, true);
    lbl->setHorizontalAlign(NVG_ALIGN_LEFT);
    this->addView(lbl, false);

    // Visible while the Sampler thread hasn't returned its first snapshot yet.
    // Without this the screen stays empty for ~1 second on entry and users
    // think the tab is hung (the original "ожидание данных навсегда" report).
    this->loadingLabel = new brls::Label(
        brls::LabelStyle::REGULAR,
        "menus/ryazhenka/status_collecting"_i18n,
        true);
    this->loadingLabel->setHorizontalAlign(NVG_ALIGN_CENTER);
    this->addView(this->loadingLabel, false);

    try {
        this->addView(
            makeChartRow(chart::makeChartFor(metrics::Metric::CpuTempC),
                         chart::makeChartFor(metrics::Metric::SkinTempC)),
            true);
        this->addView(
            makeChartRow(chart::makeChartFor(metrics::Metric::BatteryPct),
                         chart::makeChartFor(metrics::Metric::SdFreePct)),
            true);
    } catch (...) {
        auto* fallback = new brls::Label(
            brls::LabelStyle::REGULAR,
            "menus/ryazhenka/status_charts_unavailable"_i18n,
            true);
        this->addView(fallback, false);
    }

    // B = back. Earlier `return true` without popView() consumed the event
    // and trapped the user on this view — that is what the report
    // "не могу выйти ни на какую кнопку" was about.
    this->registerAction("brls/hints/back"_i18n, brls::Key::B, [] {
        brls::Application::popView();
        return true;
    });
}

void StatusTab::willAppear(bool resetState) {
    brls::BoxLayout::willAppear(resetState);
    metrics::Sampler::instance().start();
    log::info("status_tab: visible — sampler started");
}

void StatusTab::willDisappear(bool resetState) {
    metrics::Sampler::instance().stop();
    log::info("status_tab: hidden — sampler stopped");
    brls::BoxLayout::willDisappear(resetState);
}

void StatusTab::frame(brls::FrameContext* ctx) {
    if (this->loadingLabel) {
        const auto snap = metrics::Sampler::instance().lastSnapshot();
        // Hide the placeholder as soon as any service reports data, OR after a
        // ~3 s grace period (≈180 frames @ 60 fps) so the dashboard always
        // reveals its charts even when psm/ts never flip the "ok" flags.
        if (snap.battery_ok || snap.thermal_ok || snap.storage_ok || ++this->loading_frames_ > 180) {
            this->loadingLabel->setText("");
            this->loadingLabel = nullptr;
        }
    }
    brls::BoxLayout::frame(ctx);
}

} // namespace ryazhenka

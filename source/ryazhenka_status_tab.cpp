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

    // ── Header sysinfo line ──────────────────────────────────────────
    {
        const auto snap = sysinfo::collect();
        const std::string header = sysinfo::formatOneLine(snap);
        auto* lbl = new brls::Label(
            brls::LabelStyle::REGULAR,
            "menus/ryazhenka/status_header"_i18n + "  —  " + header,
            true);
        lbl->setHorizontalAlign(NVG_ALIGN_LEFT);
        this->addView(lbl, false);
    }

    // ── 2x2 grid of charts ───────────────────────────────────────────
    this->addView(
        makeChartRow(chart::makeChartFor(metrics::Metric::CpuTempC),
                     chart::makeChartFor(metrics::Metric::SkinTempC)),
        true /* fill */);
    this->addView(
        makeChartRow(chart::makeChartFor(metrics::Metric::BatteryPct),
                     chart::makeChartFor(metrics::Metric::SdFreePct)),
        true);

    this->registerAction("", brls::Key::B, [] { return true; });
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

} // namespace ryazhenka

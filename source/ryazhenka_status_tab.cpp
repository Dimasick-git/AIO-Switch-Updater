/**
 * @file ryazhenka_status_tab.cpp
 * @brief See ryazhenka_status_tab.hpp.
 * @author Dimasick-git
 */

#include "ryazhenka_status_tab.hpp"

#include <string>

#include "ryazhenka_chart_view.hpp"
#include "ryazhenka_health.hpp"
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

    // Deferred sysinfo: collect() does two directory_iterator scans of
    // /atmosphere/contents and /atmosphere/exefs_patches which can take
    // seconds on a packed SD. Build the header label with a placeholder so
    // ctor stays cheap; refreshHeader() fills it in from willAppear().
    this->headerLabel = new brls::Label(
        brls::LabelStyle::REGULAR,
        std::string("menus/ryazhenka/status_header"_i18n) + "  —  …",
        true);
    this->headerLabel->setHorizontalAlign(NVG_ALIGN_LEFT);
    this->addView(this->headerLabel, false);

    // CFW health summary card (filled in willAppear / refreshHealth so the
    // filesystem checks don't run at MainFrame construction time).
    this->healthCard = new RyazhenkaCard("menus/ryazhenka/health_title"_i18n,
                                         "menus/ryazhenka/status_collecting"_i18n, "", "");
    this->healthCard->getClickEvent()->subscribe([this](brls::View*) { this->refreshHealth(); });
    this->addView(this->healthCard, false);

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

    // B = back. When this view is a root view pushed from the Tools tab it has
    // no parent, so we pop it ourselves (returning true consumes B). When it is
    // the content of the dashboard *tab* it has a parent, so we return false and
    // let the TabFrame's own B handler return focus to the sidebar — otherwise
    // popView() would tear down the whole MainFrame.
    this->registerAction("brls/hints/back"_i18n, brls::Key::B, [this] {
        if (this->hasParent())
            return false;
        brls::Application::popView();
        return true;
    });
}

void StatusTab::refreshHeader() {
    if (!this->headerLabel)
        return;
    std::string header = "menus/ryazhenka/status_header"_i18n;
    try {
        const auto snap = sysinfo::collect();
        header += "  —  " + sysinfo::formatOneLine(snap);
    } catch (...) {
        header += "  —  (sysinfo unavailable)";
    }
    this->headerLabel->setText(header);
}

void StatusTab::refreshHealth() {
    if (!this->healthCard)
        return;
    try {
        const auto issues = health::run();
        const auto worst = health::worst(issues);
        int warnings = 0, errors = 0;
        for (const auto& i : issues) {
            if (i.severity == health::Severity::error) ++errors;
            else if (i.severity == health::Severity::warn) ++warnings;
        }
        switch (worst) {
            case health::Severity::error:
                this->healthCard->setValue("menus/ryazhenka/health_error"_i18n);
                break;
            case health::Severity::warn:
                this->healthCard->setValue("menus/ryazhenka/health_warn"_i18n);
                break;
            default:
                this->healthCard->setValue("menus/ryazhenka/health_ok"_i18n);
                break;
        }
        this->healthCard->setSubtitle(
            std::to_string(warnings) + " warn   " + std::to_string(errors) + " err");
    } catch (...) {
        this->healthCard->setValue("");
        this->healthCard->setSubtitle("menus/ryazhenka/status_charts_unavailable"_i18n);
    }
}

void StatusTab::willAppear(bool resetState) {
    brls::BoxLayout::willAppear(resetState);
    metrics::Sampler::instance().start();
    this->refreshHeader();   // deferred sysinfo::collect — see ctor
    this->refreshHealth();
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

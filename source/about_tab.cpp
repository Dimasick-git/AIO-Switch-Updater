#include "about_tab.hpp"

#include <string>

#include "ryazhenka_config.hpp"
#include "ryazhenka_health.hpp"

namespace i18n = brls::i18n;
using namespace i18n::literals;

AboutTab::AboutTab()
{
    // Subtitle
    brls::Label* subTitle = new brls::Label(
        brls::LabelStyle::REGULAR,
        "menus/about/title"_i18n,
        true);
    subTitle->setHorizontalAlign(NVG_ALIGN_CENTER);
    this->addView(subTitle);

    // Copyright
    brls::Label* copyright = new brls::Label(
        brls::LabelStyle::DESCRIPTION,
        "menus/about/copyright"_i18n + "\n© 2020-2022 HamletDuFromage\n© 2025-2026 Dimasick-git (Ryazhenka rework)",
        true);
    copyright->setHorizontalAlign(NVG_ALIGN_CENTER);
    this->addView(copyright);

    // CFW health snapshot
    this->addView(new brls::Header("menus/ryazhenka/health_title"_i18n));
    {
        std::string healthText;
        for (const auto& issue : ryazhenka::health::run()) {
            const char* tag = "?";
            switch (issue.severity) {
                case ryazhenka::health::Severity::ok:    tag = "[OK]";    break;
                case ryazhenka::health::Severity::warn:  tag = "[WARN]";  break;
                case ryazhenka::health::Severity::error: tag = "[ERROR]"; break;
            }
            healthText.append(tag);
            healthText.append(" ");
            healthText.append(issue.title);
            if (!issue.detail.empty()) {
                healthText.append(": ");
                healthText.append(issue.detail);
            }
            healthText.push_back('\n');
        }
        brls::Label* healthLabel = new brls::Label(
            brls::LabelStyle::SMALL,
            healthText,
            true);
        this->addView(healthLabel);
    }

    // Ryazhenka ecosystem links — visible on About tab so users can find the
    // companion repos shipped with the pack without having to remember URLs.
    this->addView(new brls::Header("menus/ryazhenka/ecosystem_title"_i18n));
    std::string ecosystem;
    for (const auto& link : ryazhenka::kEcosystemLinks) {
        ecosystem.append("• ");
        ecosystem.append(link.name);
        ecosystem.append("  —  ");
        ecosystem.append(link.url);
        ecosystem.push_back('\n');
    }
    brls::Label* ecoLabel = new brls::Label(
        brls::LabelStyle::SMALL,
        ecosystem,
        true);
    this->addView(ecoLabel);

    // Original disclaimers
    this->addView(new brls::Header("menus/about/disclaimers_title"_i18n));
    brls::Label* links = new brls::Label(
        brls::LabelStyle::SMALL,
        "menus/about/disclaimers"_i18n + "\n" + "menus/about/donate"_i18n,
        true);
    this->addView(links);
}
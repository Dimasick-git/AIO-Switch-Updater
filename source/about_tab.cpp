#include "about_tab.hpp"

#include <string>

#include "constants.hpp"
#include "ryazhenka_banner.hpp"
#include "ryazhenka_config.hpp"
#include "ryazhenka_haptics.hpp"
#include "utils.hpp"

namespace i18n = brls::i18n;
using namespace i18n::literals;

AboutTab::AboutTab()
{
    // Per-release banner (bbootlogo.png on the Ryzhenka GitHub release).
    // Cached-only: AboutTab is the FIRST tab MainFrame addTabs (its
    // willAppear fires during construction), so a detached-thread refresh
    // here would reintroduce the startup-crash regression that ca62519
    // ripped out. The banner is refreshed elsewhere (Settings "refresh
    // banner" action) — here we only show whatever is already on disk.
    if (brls::View* banner = ryazhenka::banner::makeBannerView()) {
        this->addView(banner);
    }

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

    // Ryazhenka ecosystem links — visible on About tab so users can find the
    // companion repos shipped with the pack without having to remember URLs.
    // (No network calls here — the synchronous health::run() that previously
    // sat in this ctor blocked the first frame on a curl HTTPS request to
    // GitHub, which is why the app appeared to crash on launch.)
    this->addView(new brls::Header("menus/ryazhenka/ecosystem_title"_i18n));
    for (const auto& link : ryazhenka::kEcosystemLinks) {
        brls::ListItem* card = new brls::ListItem(std::string(link.name), std::string(link.url));
        card->setHeight(LISTITEM_HEIGHT);
        // Open the link in the system applet browser instead of just buzzing.
        // util::openWebBrowser pops the Switch's own browser-applet, so users
        // can read the repo / Telegram channel without leaving the app.
        const std::string captured(link.url);
        card->getClickEvent()->subscribe([captured](brls::View*) {
            util::openWebBrowser(captured);
            ryazhenka::haptics::success();
        });
        this->addView(card);
    }

    // Disclaimers (donate link removed at user's request).
    this->addView(new brls::Header("menus/about/disclaimers_title"_i18n));
    brls::Label* links = new brls::Label(
        brls::LabelStyle::SMALL,
        "menus/about/disclaimers"_i18n,
        true);
    this->addView(links);
}
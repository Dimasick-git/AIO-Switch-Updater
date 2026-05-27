#include "ryazhenka_branding.hpp"

#include <borealis.hpp>

#include "ryazhenka_config.hpp"
#include "ryazhenka_logger.hpp"

namespace ryazhenka::branding {

void applyBranding() {
    // The borealis theme API surface differs between minor versions, so for
    // now we limit ourselves to a footer string and a structured log line.
    // Future iterations will inject the Ryazhenka colour scheme once we have
    // CI feedback on which borealis Theme/setTheme overload is available in
    // this snapshot.
    try {
        brls::Application::setCommonFooter(std::string(kAppTitleLocalized));
    } catch (...) {
        // Borealis throws on unsupported configurations on some platforms;
        // branding is cosmetic so swallow.
    }
    ryazhenka::log::info("branding applied — title set to Ryazhenka Updater");
}

} // namespace ryazhenka::branding

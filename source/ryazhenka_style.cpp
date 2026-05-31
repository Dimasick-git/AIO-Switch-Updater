#include "ryazhenka_style.hpp"

namespace ryazhenka {

RyazhenkaStyle::RyazhenkaStyle() : brls::HorizonStyle() {
    // Sidebar width — was 410, the user complained the focus highlight got
    // clipped on long Russian labels ("Обновление Atmosphere" etc.), so we
    // give the sidebar a bit more room back: 340 fits the longest labels
    // we ship and still leaves the content area noticeably wider than
    // HorizonStyle.
    this->Sidebar.width = 340;
    this->Sidebar.marginLeft   = 32;
    this->Sidebar.marginRight  = 20;
    this->Sidebar.marginTop    = 24;
    this->Sidebar.marginBottom = 24;

    // Shorter sidebar items so all 9 Ryazhenka tabs fit vertically without
    // the bottom one ("Состояние") falling off the screen. Was 70 px tall;
    // 9 × 56 + 40 = 544 px fits easily within the 720 px window.
    this->Sidebar.Item.height        = 56;
    this->Sidebar.Item.textSize      = 20;
    this->Sidebar.Item.padding       = 7;
    this->Sidebar.Item.textOffsetX   = 12;

    // The separator between tab groups gets the same vertical-budget cut.
    this->Sidebar.Separator.height = 18;
}

}  // namespace ryazhenka

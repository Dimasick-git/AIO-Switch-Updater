#include "ryazhenka_style.hpp"

namespace ryazhenka {

RyazhenkaStyle::RyazhenkaStyle() : brls::HorizonStyle() {
    // Narrower sidebar — was 410 px, leaves more room for the right-pane
    // text + value highlights that the user kept seeing cut off.
    this->Sidebar.width = 290;
    this->Sidebar.marginLeft   = 28;
    this->Sidebar.marginRight  = 16;
    this->Sidebar.marginTop    = 20;
    this->Sidebar.marginBottom = 20;

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

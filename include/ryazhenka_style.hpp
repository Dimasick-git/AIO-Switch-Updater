#pragma once

/**
 * @file ryazhenka_style.hpp
 * @brief brls::Style override — narrower sidebar so tabs fit on screen.
 * @author Dimasick-git
 *
 * HorizonStyle's defaults give the sidebar a width of 410 px and an item
 * height of 70 px. On a Switch with 9+ Ryazhenka tabs that overflows the
 * screen vertically (the bottom tab gets clipped) AND eats so much
 * horizontal space that the right-pane content (text, values, highlights)
 * gets cut off — exactly the layout problem the user is reporting.
 *
 * We just override the few sidebar dimensions that matter; everything else
 * inherits straight from HorizonStyle.
 */

#include <borealis.hpp>

namespace ryazhenka {

class RyazhenkaStyle : public brls::HorizonStyle {
public:
    RyazhenkaStyle();
};

}  // namespace ryazhenka

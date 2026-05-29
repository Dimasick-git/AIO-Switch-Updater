#pragma once

/**
 * @file ryazhenka_tab_frame.hpp
 * @brief Tab frame with an animated fade-in on each tab switch.
 * @author Dimasick-git
 *
 * Mirrors brls::TabFrame (sidebar + right pane), but routes tab switches
 * through our own switchToView() so we can animate the incoming pane's alpha.
 * We rebuild the sidebar/box-layout pair from scratch instead of subclassing
 * brls::TabFrame because TabFrame::switchToView and its rightPane/layout
 * members are private — there is no protected hook to override.
 */

#include <borealis.hpp>
#include <string>

namespace ryazhenka {

class RyazhenkaTabFrame : public brls::AppletFrame {
public:
    RyazhenkaTabFrame();

    /// Drop-in replacement for brls::TabFrame::addTab().
    void addTab(std::string label, brls::View* view);
    void addSeparator();

    brls::View* getDefaultFocus() override;
    bool onCancel() override;

    ~RyazhenkaTabFrame();

private:
    brls::Sidebar* sidebar = nullptr;
    brls::BoxLayout* layoutBox = nullptr;
    brls::View* currentPane = nullptr;

    void switchToView(brls::View* view);
};

}  // namespace ryazhenka

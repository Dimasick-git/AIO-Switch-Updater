#pragma once

/**
 * @file ryazhenka_card.hpp
 * @brief Focusable, glowing card view — the building block of the new UI.
 * @author Dimasick-git
 *
 * A custom-drawn alternative to brls::ListItem. It is a focusable leaf View, so
 * it can be dropped straight into a brls::List (which keeps handling vertical
 * focus + scrolling) while giving us rounded translucent cards, a focus glow,
 * an optional material icon, and rumble feedback.
 */

#include <borealis.hpp>
#include <string>

namespace ryazhenka {

class RyazhenkaCard : public brls::View {
public:
    RyazhenkaCard(std::string title, std::string subtitle = "",
                  std::string iconGlyph = "", std::string value = "");

    // brls::View
    void draw(NVGcontext* vg, int x, int y, unsigned width, unsigned height,
              brls::Style* style, brls::FrameContext* ctx) override;
    brls::View* getDefaultFocus() override { return this; }
    void onFocusGained() override;
    void onFocusLost() override;

    /// Subscribe to clicks (A button), like brls::ListItem::getClickEvent().
    brls::GenericEvent* getClickEvent() { return &this->clickEvent; }

    void setValue(const std::string& value);
    void setSubtitle(const std::string& subtitle);

    /// Drop-in compatibility with brls::ListItem::getLabel() — returns the title.
    const std::string& getLabel() const { return this->title; }

private:
    bool onClick();
    void animateGlow(float target);

    std::string title;
    std::string subtitle;
    std::string icon;
    std::string value;
    float glow = 0.0f;  // 0..1 focus glow, animated
    brls::GenericEvent clickEvent;
};

}  // namespace ryazhenka

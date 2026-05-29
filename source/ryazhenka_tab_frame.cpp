#include "ryazhenka_tab_frame.hpp"

namespace ryazhenka {

RyazhenkaTabFrame::RyazhenkaTabFrame()
    : brls::AppletFrame(false, true) {
    // Mirror brls::TabFrame's structure: a horizontal box with the sidebar on
    // the left and the active tab's pane on the right.
    this->sidebar = new brls::Sidebar();

    this->layoutBox = new brls::BoxLayout(brls::BoxLayoutOrientation::HORIZONTAL);
    this->layoutBox->addView(this->sidebar);

    this->setContentView(this->layoutBox);
}

void RyazhenkaTabFrame::addTab(std::string label, brls::View* view) {
    brls::SidebarItem* item = this->sidebar->addItem(label, view);
    item->getFocusEvent()->subscribe([this](brls::View* v) {
        if (auto* si = dynamic_cast<brls::SidebarItem*>(v))
            this->switchToView(si->getAssociatedView());
    });

    // Show the first tab eagerly so the right pane isn't empty before any
    // sidebar item ever gets focus.
    if (!this->currentPane)
        this->switchToView(view);
}

void RyazhenkaTabFrame::addSeparator() {
    this->sidebar->addSeparator();
}

void RyazhenkaTabFrame::switchToView(brls::View* view) {
    if (this->currentPane == view)
        return;

    // Drop the outgoing pane (no animation — the new one is the focal point).
    if (this->layoutBox->getViewsCount() > 1) {
        if (this->currentPane)
            this->currentPane->willDisappear(true);
        this->layoutBox->removeView(1, false);
    }

    this->currentPane = view;
    if (view) {
        // addView() calls view->willAppear(true).
        this->layoutBox->addView(view, true, true);

        // Fade the new pane in over ~180 ms via View::alpha. Cheap (one
        // nvgGlobalAlpha applied inside View::frame()) and matches the
        // RyazhenkaCard focus-glow timing for a consistent feel.
        view->alpha = 0.0f;
        brls::menu_animation_ctx_tag tag = (uintptr_t)&view->alpha;
        brls::menu_animation_kill_by_tag(&tag);

        brls::menu_animation_ctx_entry_t entry;
        entry.easing_enum  = brls::EASING_OUT_QUAD;
        entry.tag          = tag;
        entry.duration     = 180.0f;
        entry.target_value = 1.0f;
        entry.subject      = &view->alpha;
        entry.cb           = [](void*) {};
        entry.tick         = [](void*) {};
        entry.userdata     = nullptr;
        brls::menu_animation_push(&entry);
    }
}

brls::View* RyazhenkaTabFrame::getDefaultFocus() {
    if (this->currentPane) {
        if (auto* f = this->currentPane->getDefaultFocus())
            return f;
    }
    return this->sidebar->getDefaultFocus();
}

bool RyazhenkaTabFrame::onCancel() {
    // Mirror brls::TabFrame::onCancel: a B press from inside a pane should
    // hop focus back to the sidebar instead of popping the frame.
    if (!this->sidebar->isChildFocused()) {
        brls::Application::onGamepadButtonPressed(GLFW_GAMEPAD_BUTTON_DPAD_LEFT, false);
        return true;
    }
    return brls::AppletFrame::onCancel();
}

RyazhenkaTabFrame::~RyazhenkaTabFrame() {
    // Content view (layoutBox + sidebar) is owned by AppletFrame and freed by
    // its dtor; the current pane was inserted there too, so don't double-free.
    this->switchToView(nullptr);
}

}  // namespace ryazhenka

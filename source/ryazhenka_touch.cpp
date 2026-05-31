#include "ryazhenka_touch.hpp"

#include <atomic>
#include <borealis.hpp>

#ifdef __SWITCH__
#include <switch.h>
#define GLFW_INCLUDE_NONE
#include <GLFW/glfw3.h>
#endif

#include "constants.hpp"
#include "fs.hpp"
#include "ryazhenka_logger.hpp"

namespace ryazhenka::touch {

namespace {

std::atomic<bool> g_enabled{true};

#ifdef __SWITCH__

bool g_inited = false;

// Edge-detection state for "tap-release" — we fire on release, not on press,
// so that a drag (e.g. a user-cancelled tap) doesn't trigger an action.
bool g_wasDown = false;
int  g_lastX = 0;
int  g_lastY = 0;
// Where the touch started — used to distinguish a tap from a swipe / drag.
int  g_pressX = 0;
int  g_pressY = 0;

// Max distance (in screen pixels) a finger can travel between press and
// release for the gesture to still count as a "tap" rather than a "swipe".
// Switch screen is 1280×720, so 40 px is roughly one card's height.
constexpr int kTapSlopPx = 40;

void onTapRelease(int x, int y) {
    // brls::Application::windowWidth/Height are private, but on Switch the
    // physical screen (1280×720) and the brls content surface are 1:1, so we
    // just treat touch coordinates as content coordinates. If the content
    // surface ever stops matching the physical screen, scale by
    // contentWidth/720 here.
    const float cx = static_cast<float>(x);
    const float cy = static_cast<float>(y);
    (void)brls::Application::contentWidth;   // keep the symbol referenced so
    (void)brls::Application::contentHeight;  // future scaling has a hook.

    // 2) Decide where on screen the tap landed. The sidebar is on the left
    //    third of the screen; everything else is content. We don't have
    //    proper hit-testing (would need view-tree access we don't have),
    //    so the tap walks focus using the same D-pad path the user would.
    auto* style = brls::Application::getStyle();
    if (!style)
        return;

    const float sidebarRight = static_cast<float>(style->Sidebar.width);
    const bool inSidebar = cx < sidebarRight;

    if (inSidebar) {
        // Sidebar tap — move focus left until we're in the sidebar, then up
        // or down toward the target Y. A safety cap of 12 navigation steps
        // prevents an infinite loop if focus traversal gets stuck.
        const int marginTop = static_cast<int>(style->Sidebar.marginTop);
        const int itemH     = std::max(8, static_cast<int>(style->Sidebar.Item.height));
        const int targetIdx = std::max(0, (static_cast<int>(cy) - marginTop) / itemH);

        // Hop into the sidebar from wherever we are.
        for (int i = 0; i < 6; ++i) {
            brls::View* focus = brls::Application::getCurrentFocus();
            if (focus && focus->getX() < static_cast<int>(sidebarRight))
                break;  // already inside the sidebar
            brls::Application::onGamepadButtonPressed(GLFW_GAMEPAD_BUTTON_DPAD_LEFT, false);
        }

        // Now walk vertically toward targetIdx. We don't know the current
        // sidebar index, so we estimate from current focus Y and step
        // toward the target.
        for (int i = 0; i < 12; ++i) {
            brls::View* focus = brls::Application::getCurrentFocus();
            if (!focus) break;
            const int currentY = focus->getY();
            const int currentIdx = std::max(0, (currentY - marginTop) / itemH);
            if (currentIdx == targetIdx)
                break;
            const auto dir = currentIdx < targetIdx
                                 ? GLFW_GAMEPAD_BUTTON_DPAD_DOWN
                                 : GLFW_GAMEPAD_BUTTON_DPAD_UP;
            brls::Application::onGamepadButtonPressed(dir, false);
        }
        return;
    }

    // Content tap — fire A on whatever is currently focused. This works
    // because every interactive view (RyazhenkaCard, brls::ListItem,
    // toggles, …) consumes A as its primary action. The user still has
    // to D-pad to the item they want; touch is the "confirm" gesture.
    brls::Application::onGamepadButtonPressed(GLFW_GAMEPAD_BUTTON_A, false);
}

#endif  // __SWITCH__

}  // namespace

void init() {
    try {
        nlohmann::ordered_json cfg = fs::parseJsonFile(CONFIG_FILE);
        if (cfg.is_object() && cfg.contains("ryazhenka_touch_enabled") &&
            cfg["ryazhenka_touch_enabled"].is_boolean())
            g_enabled.store(cfg["ryazhenka_touch_enabled"].get<bool>());
    } catch (...) { /* keep default */ }

#ifdef __SWITCH__
    // hid is already initialised by GLFW's Switch backend — nothing extra to
    // do here, we just poll it from tick().
    g_inited = true;
    log::info(std::string("touch: ready enabled=") + (g_enabled.load() ? "yes" : "no"));
#else
    log::info("touch: stub (non-switch build)");
#endif
}

void tick() {
    if (!g_enabled.load())
        return;
#ifdef __SWITCH__
    if (!g_inited)
        return;

    HidTouchScreenState state{};
    if (!hidGetTouchScreenStates(&state, 1))
        return;

    const bool nowDown = state.count > 0;
    if (nowDown) {
        g_lastX = state.touches[0].x;
        g_lastY = state.touches[0].y;
        if (!g_wasDown) {
            g_pressX = g_lastX;
            g_pressY = g_lastY;
        }
    }

    if (!nowDown && g_wasDown) {
        // Released: tap only if the finger stayed within slop of where it
        // landed (don't fire on a swipe).
        const int dx = g_lastX - g_pressX;
        const int dy = g_lastY - g_pressY;
        if (dx * dx + dy * dy <= kTapSlopPx * kTapSlopPx)
            onTapRelease(g_lastX, g_lastY);
    }
    g_wasDown = nowDown;
#endif
}

void setEnabled(bool enabled) { g_enabled.store(enabled); }
bool isEnabled() { return g_enabled.load(); }

}  // namespace ryazhenka::touch

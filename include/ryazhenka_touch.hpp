#pragma once

/**
 * @file ryazhenka_touch.hpp
 * @brief Minimal touch input — turns a tap into a press-A on current focus.
 * @author Dimasick-git
 *
 * borealis (this version, vendored as a submodule) has no touch / mouse
 * pipeline. Adding a fully-featured view-tree hit-tester would require
 * surgery on brls::View itself; this module ships the MVP instead: poll
 * libnx HidTouchScreenState once per frame and on tap-release synthesise
 * the equivalent of an A-button press on whichever view is currently
 * focused. The user navigates with the D-pad to set focus, then taps the
 * screen anywhere to confirm — every existing click handler in the app
 * runs untouched.
 */

namespace ryazhenka::touch {

/// Initialises the touch screen polling (libnx hid backend, applet-mode
/// safe — silently no-ops on non-Switch / failed init).
void init();

/// Per-frame tick — call from a hot path that runs every frame. We hang
/// off WaveBackground::preFrame to keep it next to the other tickers.
void tick();

/// Runtime toggle. Persisted by the caller. Default ON.
void setEnabled(bool enabled);
bool isEnabled();

}  // namespace ryazhenka::touch

#pragma once

/**
 * @file ryazhenka_haptics.hpp
 * @brief Joy-Con / Pro Controller rumble feedback.
 * @author Dimasick-git
 *
 * Frame-driven, non-blocking: pulse() starts a short rumble and records when it
 * should stop; tick() (called once per frame from the background) sends the
 * zero-amplitude stop and runs any queued follow-up buzz. No worker threads, no
 * blocking the UI thread. All libnx calls are #ifdef __SWITCH__ so the module is
 * a harmless no-op on PC / emulators that don't implement vibration.
 */

namespace ryazhenka::haptics {

/// Initialises vibration devices (handheld + detached pair) and reads the
/// haptics_enabled / haptics_strength keys from config.json. Safe to call once
/// after brls::Application::init().
void init();

/// Stops any active rumble. Call before tearing down HID at app exit.
void exit();

/// Pumps the scheduler: stops expired rumbles and fires queued follow-ups.
/// Must be called once per frame.
void tick();

/// Starts a rumble at `intensity` (0..1, scaled by the user strength setting)
/// for `duration_ms`. Overlapping calls simply extend/replace the active buzz.
void pulse(float intensity, int duration_ms);

// Semantic feedback levels used across the UI.
void focus();    ///< light tap when focus moves
void click();    ///< medium buzz on A / confirm
void success();  ///< strong buzz when an action completes
void error();    ///< double buzz on failure / cancel

/// Runtime toggles (used by the settings screen). Persisted by the caller.
void setEnabled(bool enabled);
bool isEnabled();
void setStrength(float strength);  ///< clamped to 0..1

}  // namespace ryazhenka::haptics

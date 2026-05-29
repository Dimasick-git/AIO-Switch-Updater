#pragma once

/**
 * @file ryazhenka_audio.hpp
 * @brief Procedural UI sound effects via libnx audout.
 * @author Dimasick-git
 *
 * Mirrors the haptics API: small short tones on focus / click / success /
 * error, each rendered procedurally as a sine wave with an attack-decay
 * envelope. No external audio files, no extra link dependencies — just
 * libnx audout. Every libnx call is guarded by __SWITCH__ and a runtime
 * "initialised" flag, so the module is a harmless no-op on PC / emulators
 * and on devices where audout is unavailable.
 *
 * Off by default (the user enables it from Settings). Honours the
 * ryazhenka_audio_enabled key in config.json.
 */

namespace ryazhenka::audio {

/// Initialises libnx audout and the procedural-tone buffers. Safe to call once
/// after socket / pl / setsys init. Failures are logged and disable the module
/// for the rest of the session.
void init();

/// Stops playback and tears the audout output down.
void exit();

/// Plays the named tone (non-blocking). Drops the request when disabled or
/// when the audout queue is full.
void focus();
void click();
void success();
void error();

/// Runtime toggle (Settings screen). Persisted by the caller.
void setEnabled(bool enabled);
bool isEnabled();

}  // namespace ryazhenka::audio

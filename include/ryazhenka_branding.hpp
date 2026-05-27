#pragma once

// Brand-application helpers used to inject Ryazhenka identity into the upstream
// borealis app shell. Definitions live in source/ryazhenka_branding.cpp and are
// wired up from a small upstream patch in main.cpp.

namespace ryazhenka::branding {

// Called once after brls::Application::init(). Sets accent theme, window title,
// and common footer. Safe to call when the appropriate borealis APIs are not
// available — no-ops gracefully.
void applyBranding();

} // namespace ryazhenka::branding

#pragma once

// Install a std::terminate handler that logs the active exception (when any)
// to the Ryazhenka log file before calling std::abort. Without it an
// uncaught throw turns into a silent crash on Switch hardware, which is
// painful to diagnose against the borealis console only.

namespace ryazhenka::crash {

void install();

} // namespace ryazhenka::crash

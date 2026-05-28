#include "ryazhenka_crash_handler.hpp"

#include <cstdlib>
#include <exception>
#include <string>

#include "ryazhenka_logger.hpp"

namespace ryazhenka::crash {

namespace {

[[noreturn]] void terminateHandler() {
    try {
        if (auto ex = std::current_exception()) {
            try {
                std::rethrow_exception(ex);
            } catch (const std::exception& e) {
                ryazhenka::log::error(std::string("std::terminate uncaught: ") + e.what());
            } catch (...) {
                ryazhenka::log::error("std::terminate uncaught: non-std exception");
            }
        } else {
            ryazhenka::log::error("std::terminate with no active exception");
        }
    } catch (...) {
        // Logger itself failed — nothing more we can salvage.
    }
    // _Exit rather than abort: leaves the SD-card log file in a flushed
    // state and returns the user cleanly to HOME, instead of triggering
    // the Switch crash dialog with a useless register dump.
    std::_Exit(EXIT_FAILURE);
}

} // namespace

void install() {
    std::set_terminate(&terminateHandler);
    try { ryazhenka::log::debug("crash handler installed"); } catch (...) {}
}

} // namespace ryazhenka::crash

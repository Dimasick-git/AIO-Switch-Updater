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
        // Last-ditch swallow — logger itself raised, nothing useful left to do.
    }
    std::abort();
}

} // namespace

void install() {
    std::set_terminate(&terminateHandler);
    ryazhenka::log::debug("crash handler installed");
}

} // namespace ryazhenka::crash

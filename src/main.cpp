#include "application.h"
#include <spdlog/spdlog.h>
#include <iostream>

int main(int /*argc*/, char** /*argv*/) {
    try {
        spdlog::set_level(spdlog::level::info);
        spdlog::info("P2P Pipeline Manager starting up...");

        p2p::Application app;
        if (!app.initialize(1280, 720, "P2P Pipeline Manager")) {
            spdlog::error("Failed to initialize application");
            return 1;
        }

        app.run();

        app.shutdown();
        spdlog::info("P2P Pipeline Manager shutting down gracefully");
        return 0;
    }
    catch (const std::exception& e) {
        spdlog::critical("Unhandled exception: {}", e.what());
        return 1;
    }
    catch (...) {
        spdlog::critical("Unknown exception occurred");
        return 1;
    }
}
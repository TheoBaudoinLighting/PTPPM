#include "server.h"
#include "client.h"
#include "gui.h"
#include "config.h"
#include <spdlog/spdlog.h>
#include <spdlog/sinks/basic_file_sink.h>
#include <atomic>
#include <memory>
#include <stdexcept>

int main() {
    try {
        auto file_logger = spdlog::basic_logger_mt("file_logger", "tcp_server.log");
        spdlog::set_default_logger(file_logger);
        spdlog::set_level(spdlog::level::info);
        spdlog::flush_on(spdlog::level::info);
        spdlog::info("Application démarrée");
        std::atomic<bool> running{true};
        auto server = std::make_unique<TCPServer>(DEFAULT_TCP_PORT);
        GUI gui(running, server);
        gui.run();
        spdlog::info("Application arrêtée proprement");
        return 0;
    }
    catch (const std::exception& e) {
        spdlog::critical("Erreur fatale: {}", e.what());
        return 1;
    }
}

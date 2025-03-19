#include "config.h"
#include "network_manager.h"
#include "ui_manager.h"
#include <spdlog/spdlog.h>
#include <spdlog/sinks/basic_file_sink.h>
#include <atomic>
#include <memory>
#include <stdexcept>
#include <iostream>

int main() {
    try {
        auto fileLogger = spdlog::basic_logger_mt("file_logger", "ptppm_application.log");
        spdlog::set_default_logger(fileLogger);
        
        switch (Config::DEFAULT_LOG_LEVEL) {
            case 0: spdlog::set_level(spdlog::level::trace); break;
            case 1: spdlog::set_level(spdlog::level::debug); break;
            case 2: spdlog::set_level(spdlog::level::info); break;
            case 3: spdlog::set_level(spdlog::level::warn); break;
            case 4: spdlog::set_level(spdlog::level::err); break;
            case 5: spdlog::set_level(spdlog::level::critical); break;
            default: spdlog::set_level(spdlog::level::info);
        }
        
        spdlog::flush_on(spdlog::level::info);
        spdlog::info("Application démarrée");
        
        std::atomic<bool> running{true};
        
        auto networkManager = std::make_unique<NetworkManager>();
        auto uiManager = std::make_unique<UIManager>(*networkManager);

        if (!uiManager->initialize(
            Config::DEFAULT_WINDOW_WIDTH,
            Config::DEFAULT_WINDOW_HEIGHT,
            Config::DEFAULT_WINDOW_TITLE)) {
            
            spdlog::critical("Échec de l'initialisation de l'interface utilisateur");
            return 1;
        }
        
        uiManager->run(running);
        
        uiManager->cleanup();
        networkManager.reset();
        
        spdlog::info("Application arrêtée proprement");
        return 0;
    }
    catch (const std::exception& e) {
        spdlog::critical("Erreur fatale: {}", e.what());
        std::cerr << "Erreur fatale: " << e.what() << std::endl;
        return 1;
    }
    catch (...) {
        spdlog::critical("Erreur inconnue");
        std::cerr << "Erreur inconnue" << std::endl;
        return 1;
    }
}
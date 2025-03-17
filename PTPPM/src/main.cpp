#include <iostream>
#include <string>
#include <thread>
#include <chrono>
#include <csignal>
#include <atomic>
#include <vector>
#include "peer.h"

#include "logger.h"

std::atomic<bool> g_running = true;

void signalHandler(int signal) {
    spdlog::info("Received signal {}, shutting down...", signal);
    g_running = false;
}

void displayHelp() {
    spdlog::info("Available commands:");
    spdlog::info("  start <port>                    - Start server on port");
    spdlog::info("  connect <host> <port>           - Connect to a peer");
    spdlog::info("  send <peer_id> <message>        - Send message to specific peer");
    spdlog::info("  broadcast <message>             - Send message to all peers");
    spdlog::info("  connections                     - Show connected peers");
    spdlog::info("  dht enable                      - Enable DHT functionality");
    spdlog::info("  dht bootstrap <host> <port>     - Bootstrap DHT with a known node");
    spdlog::info("  dht store <key> <value>         - Store a key-value pair in the DHT");
    spdlog::info("  dht get <key>                   - Retrieve a value from the DHT");
    spdlog::info("  dht stats                       - Show DHT statistics");
    spdlog::info("  help                            - Show this help");
    spdlog::info("  exit                            - Exit the program");
}

int main(int argc, char* argv[]) {
    Logger::init("PTPPM", "logs/ptppm.log", spdlog::level::info);
    spdlog::info("Starting PTPPM application");

    std::signal(SIGINT, signalHandler);
    std::signal(SIGTERM, signalHandler);
    
    Peer peer;
    
    if (argc > 1) {
        if (std::string(argv[1]) == "server") {
            uint16_t port = 8000;
            if (argc > 2) {
                port = static_cast<uint16_t>(std::stoi(argv[2]));
            }
            
            if (!peer.startServer(port)) {
                return 1;
            }
        }
    }
    
    spdlog::info("PTPPM with DHT");
    spdlog::info("Type 'help' for available commands");
    
    while (g_running) {
        std::string line;
        std::cout << "==> ";
        std::getline(std::cin, line);
        
        if (line.empty()) continue;
        
        std::istringstream iss(line);
        std::string command;
        iss >> command;
        
        if (command == "exit") {
            break;
        } else if (command == "help") {
            displayHelp();
        } else if (command == "start") {
            uint16_t port;
            if (iss >> port) {
                if (!peer.startServer(port)) {
                    spdlog::error("Failed to start server");
                }
            } else {
                spdlog::error("Invalid port");
            }
        } else if (command == "connect") {
            std::string host;
            uint16_t port;
            if (iss >> host >> port) {
                if (!peer.connectTo(host, port)) {
                    spdlog::error("Failed to connect");
                }
            } else {
                spdlog::error("Invalid host or port");
            }
        } else if (command == "send") {
            size_t peer_id;
            std::string message;
            if (iss >> peer_id) {
                std::getline(iss >> std::ws, message);
                if (!message.empty()) {
                    peer.sendMessage(peer_id, message);
                } else {
                    spdlog::error("Empty message");
                }
            } else {
                spdlog::error("Invalid peer ID");
            }
        } else if (command == "broadcast") {
            std::string message;
            std::getline(iss >> std::ws, message);
            if (!message.empty()) {
                peer.broadcastMessage(message);
            } else {
                spdlog::error("Empty message");
            }
        } else if (command == "connections") {
            spdlog::info("Connected peers: {}", peer.connectionCount());
        } else if (command == "dht") {
            std::string dht_command;
            if (!(iss >> dht_command)) {
                spdlog::error("Invalid DHT command");
                continue;
            }
            
            if (dht_command == "enable") {
                if (peer.enableDHT()) {
                    spdlog::info("DHT enabled successfully");
                } else {
                    spdlog::error("Failed to enable DHT");
                }
            } else if (dht_command == "bootstrap") {
                std::string host;
                uint16_t port;
                if (iss >> host >> port) {
                    if (peer.bootstrapDHT(host, port)) {
                        spdlog::info("DHT bootstrapped with {}", host + ":" + std::to_string(port));
                    } else {
                        spdlog::error("Failed to bootstrap DHT");
                    }
                } else {
                    spdlog::error("Invalid host or port");
                }
            } else if (dht_command == "store") {
                std::string key, value;
                if (iss >> key) {
                    std::getline(iss >> std::ws, value);
                    if (!value.empty()) {
                        if (peer.storeDHT(key, value)) {
                            spdlog::info("Value stored in DHT");
                        } else {
                            spdlog::error("Failed to store value in DHT");
                        }
                    } else {
                        spdlog::error("Empty value");
                    }
                } else {
                    spdlog::error("Invalid key");
                }
            } else if (dht_command == "get") {
                std::string key;
                if (iss >> key) {
                    std::string value = peer.retrieveDHT(key);
                    if (!value.empty()) {
                        spdlog::info("Value retrieved from DHT: {}", value);
                    } else {
                        spdlog::info("Key not found in DHT");
                    }
                } else {
                    spdlog::error("Invalid key");
                }
            } else if (dht_command == "stats") {
                spdlog::info(peer.getDHTStats());
            } else {
                spdlog::error("Unknown DHT command: {}", dht_command);
                spdlog::error("Available DHT commands: enable, bootstrap, store, get, stats");
            }
        } else {
            spdlog::error("Unknown command: {}", command);
            spdlog::error("Type 'help' for available commands");
        }
    }
    
    if (peer.isRunning()) {
        peer.stopServer();
    }
    
    spdlog::info("Application terminated");
    return 0;
}
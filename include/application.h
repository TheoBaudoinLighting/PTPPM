#pragma once

#include <glad/glad.h>
#include <GLFW/glfw3.h>
#include <string>
#include <memory>

#include "renderer/renderer.h"
#include "ui/ui_manager.h"
#include "network/peer_manager.h"
#include "data/database.h"
#include "version_control/version_manager.h"

namespace p2p {

    class Application {
    public:
        Application();
        ~Application();

        bool initialize(int width, int height, const std::string& title);
        void run();
        void shutdown();

    private:
        GLFWwindow* m_window;
        int m_width;
        int m_height;
        std::string m_title;

        std::unique_ptr<Renderer> m_renderer;
        std::unique_ptr<UIManager> m_ui_manager;
        std::unique_ptr<PeerManager> m_peer_manager;
        std::unique_ptr<Database> m_database;
        std::unique_ptr<VersionManager> m_version_manager;

        std::string m_config_path;
        bool m_is_running;

        bool m_is_initialized;

        static void frameBufferResizeCallback(GLFWwindow* window, int width, int height);
        static void keyCallback(GLFWwindow* window, int key, int scancode, int action, int mods);
        static void mouseButtonCallback(GLFWwindow* window, int button, int action, int mods);

        void update(float delta_time);
        void render();
        void processInput();

        bool initializeGLFW();
        bool initializeGLAD();
        bool initializeImGui();
        bool initializeComponents();
    };

} // namespace p2p
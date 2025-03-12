#pragma once

#include <glad/glad.h>
#include <GLFW/glfw3.h>
#include <imgui.h>
#include <string>
#include <memory>
#include <vector>

#include "ui/panels/panel.h"
#include "data/database.h"
#include "network/peer_manager.h"
#include "version_control/version_manager.h"

namespace p2p {

    class UIManager {
    public:
        UIManager(GLFWwindow* window, Database* database, PeerManager* peer_manager, VersionManager* version_manager);
        ~UIManager();

        bool initialize();
        void shutdown();

        void render();
        void handleResize(int width, int height);

    private:
        GLFWwindow* m_window;
        Database* m_database;
        PeerManager* m_peer_manager;
        VersionManager* m_version_manager;
        std::vector<std::unique_ptr<Panel>> m_panels;
        ImGuiIO* m_io;
        ImGuiStyle m_style;

        void setupTheme();
        void setupDockspace();
        void renderMainMenuBar();
        void renderFileMenu();
        void renderEditMenu();
        void renderViewMenu();
        void renderNetworkMenu();
        void renderHelpMenu();
        void showAboutDialog();
        void showSettingsDialog();
        void showConnectDialog();
        void showNewProjectDialog();
        void showNewAssetDialog();
        void showImportDialog();
        void showExportDialog();
        bool m_show_about;
        bool m_show_settings;
        bool m_show_connect;
        bool m_show_new_project;
        bool m_show_new_asset;
        bool m_show_import;
        bool m_show_export;
    };

} // namespace p2p
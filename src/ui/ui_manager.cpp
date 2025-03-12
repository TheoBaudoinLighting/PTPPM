#include "ui/ui_manager.h"
#include <imgui_impl_glfw.h>
#include <imgui_impl_opengl3.h>
#include <spdlog/spdlog.h>

#include "ui/panels/asset_panel.h"
#include "ui/panels/version_panel.h"
#include "ui/panels/nodes_panel.h"

namespace p2p {

    UIManager::UIManager(GLFWwindow* window, Database* database, PeerManager* peer_manager, VersionManager* version_manager)
        : m_window(window),
        m_database(database),
        m_peer_manager(peer_manager),
        m_version_manager(version_manager),
        m_io(nullptr),
        m_show_about(false),
        m_show_settings(false),
        m_show_connect(false),
        m_show_new_project(false),
        m_show_new_asset(false),
        m_show_import(false),
        m_show_export(false) {
    }

    UIManager::~UIManager() {
        shutdown();
    }

    bool UIManager::initialize() {
        m_io = &ImGui::GetIO();
        setupTheme();
        m_panels.push_back(std::make_unique<AssetPanel>(m_database, m_version_manager));
        spdlog::info("UI Manager initialized successfully");
        return true;
    }

    void UIManager::shutdown() {
        m_panels.clear();
    }

    void UIManager::render() {
        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();
        setupDockspace();
        renderMainMenuBar();

        for (auto& panel : m_panels) {
            if (panel->isVisible()) {
                panel->render();
            }
        }

        if (m_show_about) showAboutDialog();
        if (m_show_settings) showSettingsDialog();
        if (m_show_connect) showConnectDialog();
        if (m_show_new_project) showNewProjectDialog();
        if (m_show_new_asset) showNewAssetDialog();
        if (m_show_import) showImportDialog();
        if (m_show_export) showExportDialog();

        ImGui::Render();
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
    }

    void UIManager::handleResize(int width, int height) {
        m_io->DisplaySize = ImVec2(static_cast<float>(width), static_cast<float>(height));
    }

    void UIManager::setupTheme() {
        ImGuiStyle& style = ImGui::GetStyle();
        style.Colors[ImGuiCol_WindowBg] = ImVec4(0.10f, 0.10f, 0.10f, 1.00f);
        style.Colors[ImGuiCol_Header] = ImVec4(0.20f, 0.20f, 0.20f, 1.00f);
        style.Colors[ImGuiCol_HeaderHovered] = ImVec4(0.30f, 0.30f, 0.30f, 1.00f);
        style.Colors[ImGuiCol_HeaderActive] = ImVec4(0.40f, 0.40f, 0.40f, 1.00f);
        style.Colors[ImGuiCol_Button] = ImVec4(0.20f, 0.20f, 0.20f, 1.00f);
        style.Colors[ImGuiCol_ButtonHovered] = ImVec4(0.30f, 0.30f, 0.30f, 1.00f);
        style.Colors[ImGuiCol_ButtonActive] = ImVec4(0.40f, 0.40f, 0.40f, 1.00f);
        style.WindowPadding = ImVec2(8.0f, 8.0f);
        style.FramePadding = ImVec2(4.0f, 4.0f);
        style.ItemSpacing = ImVec2(8.0f, 4.0f);
        style.ItemInnerSpacing = ImVec2(4.0f, 4.0f);
        style.TouchExtraPadding = ImVec2(0.0f, 0.0f);
        style.WindowBorderSize = 1.0f;
        style.ChildBorderSize = 1.0f;
        style.PopupBorderSize = 1.0f;
        style.FrameBorderSize = 1.0f;
        style.TabBorderSize = 1.0f;
        style.WindowRounding = 4.0f;
        style.ChildRounding = 4.0f;
        style.FrameRounding = 4.0f;
        style.PopupRounding = 4.0f;
        style.ScrollbarRounding = 4.0f;
        style.GrabRounding = 4.0f;
        style.TabRounding = 4.0f;
    }

    void UIManager::setupDockspace() {
        ImGuiViewport* viewport = ImGui::GetMainViewport();
        ImGuiWindowFlags window_flags = ImGuiWindowFlags_MenuBar | ImGuiWindowFlags_NoDocking;
        window_flags |= ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove;
        window_flags |= ImGuiWindowFlags_NoBringToFrontOnFocus | ImGuiWindowFlags_NoNavFocus;

        ImGui::SetNextWindowPos(viewport->Pos);
        ImGui::SetNextWindowSize(viewport->Size);
        ImGui::SetNextWindowViewport(viewport->ID);
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));
        ImGui::Begin("DockSpace", nullptr, window_flags);
        ImGui::PopStyleVar();
        ImGuiID dockspace_id = ImGui::GetID("MyDockSpace");
        ImGui::DockSpace(dockspace_id, ImVec2(0.0f, 0.0f), ImGuiDockNodeFlags_None);
        ImGui::End();
    }

    void UIManager::renderMainMenuBar() {
        if (ImGui::BeginMainMenuBar()) {
            if (ImGui::BeginMenu("File")) {
                renderFileMenu();
                ImGui::EndMenu();
            }

            if (ImGui::BeginMenu("Edit")) {
                renderEditMenu();
                ImGui::EndMenu();
            }

            if (ImGui::BeginMenu("View")) {
                renderViewMenu();
                ImGui::EndMenu();
            }

            if (ImGui::BeginMenu("Network")) {
                renderNetworkMenu();
                ImGui::EndMenu();
            }

            if (ImGui::BeginMenu("Help")) {
                renderHelpMenu();
                ImGui::EndMenu();
            }

            ImGui::EndMainMenuBar();
        }
    }

    void UIManager::renderFileMenu() {
        if (ImGui::MenuItem("New Project", "Ctrl+N")) {
            m_show_new_project = true;
        }

        if (ImGui::MenuItem("New Asset", "Ctrl+Shift+N")) {
            m_show_new_asset = true;
        }

        ImGui::Separator();

        if (ImGui::MenuItem("Import", "Ctrl+I")) {
            m_show_import = true;
        }

        if (ImGui::MenuItem("Export", "Ctrl+E")) {
            m_show_export = true;
        }

        ImGui::Separator();

        if (ImGui::MenuItem("Settings", "Ctrl+,")) {
            m_show_settings = true;
        }

        ImGui::Separator();

        if (ImGui::MenuItem("Exit", "Alt+F4")) {
            glfwSetWindowShouldClose(m_window, GLFW_TRUE);
        }
    }

    void UIManager::renderEditMenu() {
        if (ImGui::MenuItem("Undo", "Ctrl+Z")) {
        }

        if (ImGui::MenuItem("Redo", "Ctrl+Y")) {
        }

        ImGui::Separator();

        if (ImGui::MenuItem("Cut", "Ctrl+X")) {
        }

        if (ImGui::MenuItem("Copy", "Ctrl+C")) {
        }

        if (ImGui::MenuItem("Paste", "Ctrl+V")) {
        }

        ImGui::Separator();

        if (ImGui::MenuItem("Delete", "Del")) {
        }
    }

    void UIManager::renderViewMenu() {
        for (auto& panel : m_panels) {
            bool is_visible = panel->isVisible();
            if (ImGui::MenuItem(panel->getTitle().c_str(), nullptr, &is_visible)) {
                if (is_visible) {
                    panel->show();
                }
                else {
                    panel->hide();
                }
            }
        }
    }

    void UIManager::renderNetworkMenu() {
        if (ImGui::MenuItem("Connect to Peer", "Ctrl+P")) {
            m_show_connect = true;
        }

        ImGui::Separator();

        if (ImGui::MenuItem("Start Discovery", nullptr, m_peer_manager->getStatus() != PeerStatus::OFFLINE)) {
            m_peer_manager->startDiscovery();
        }

        if (ImGui::MenuItem("Stop Discovery")) {
            m_peer_manager->stopDiscovery();
        }

        ImGui::Separator();

        if (ImGui::BeginMenu("Status")) {
            PeerStatus current_status = m_peer_manager->getStatus();

            bool is_online = current_status == PeerStatus::ONLINE;
            if (ImGui::MenuItem("Online", nullptr, is_online)) {
                m_peer_manager->setStatus(PeerStatus::ONLINE);
            }

            bool is_busy = current_status == PeerStatus::BUSY;
            if (ImGui::MenuItem("Busy", nullptr, is_busy)) {
                m_peer_manager->setStatus(PeerStatus::BUSY);
            }

            bool is_away = current_status == PeerStatus::AWAY;
            if (ImGui::MenuItem("Away", nullptr, is_away)) {
                m_peer_manager->setStatus(PeerStatus::AWAY);
            }

            ImGui::EndMenu();
        }
    }

    void UIManager::renderHelpMenu() {
        if (ImGui::MenuItem("Documentation")) {
        }

        if (ImGui::MenuItem("About")) {
            m_show_about = true;
        }
    }

    void UIManager::showAboutDialog() {
        ImGui::SetNextWindowSize(ImVec2(500, 320), ImGuiCond_FirstUseEver);
        if (ImGui::Begin("About", &m_show_about, ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoCollapse)) {
            ImGui::Text("P2P Pipeline Manager");
            ImGui::Separator();
            ImGui::Text("Version: 1.0.0");
            ImGui::Text("A peer-to-peer pipeline management system for production teams");
            ImGui::Spacing();
            ImGui::Text("Built with:");
            ImGui::BulletText("GLFW");
            ImGui::BulletText("GLAD");
            ImGui::BulletText("ImGui");
            ImGui::BulletText("Asio");
            ImGui::BulletText("SQLite");
            ImGui::BulletText("nlohmann/json");
            ImGui::BulletText("spdlog");
            ImGui::Spacing();
            ImGui::Text("(C) 2025 All Rights Reserved");

            ImGui::Separator();
            if (ImGui::Button("Close", ImVec2(120, 0))) {
                m_show_about = false;
            }
        }
        ImGui::End();
    }

    void UIManager::showSettingsDialog() {
        ImGui::SetNextWindowSize(ImVec2(600, 400), ImGuiCond_FirstUseEver);
        if (ImGui::Begin("Settings", &m_show_settings)) {
            if (ImGui::BeginTabBar("SettingsTabs")) {
                if (ImGui::BeginTabItem("General")) {
                    ImGui::Text("Application Settings");
                    ImGui::Separator();

                    static char username[128] = "User";
                    ImGui::InputText("Username", username, IM_ARRAYSIZE(username));

                    static int theme = 0;
                    ImGui::Combo("Theme", &theme, "Dark\0Light\0Custom\0\0");

                    static bool auto_save = true;
                    ImGui::Checkbox("Auto Save", &auto_save);

                    static int auto_save_interval = 5;
                    ImGui::SliderInt("Auto Save Interval (minutes)", &auto_save_interval, 1, 30);

                    ImGui::EndTabItem();
                }

                if (ImGui::BeginTabItem("Network")) {
                    ImGui::Text("Network Settings");
                    ImGui::Separator();

                    static char host[128] = "0.0.0.0";
                    ImGui::InputText("Host", host, IM_ARRAYSIZE(host));

                    static int port = 12345;
                    ImGui::InputInt("Port", &port);

                    static bool auto_discovery = true;
                    ImGui::Checkbox("Auto Discovery", &auto_discovery);

                    static int discovery_interval = 30;
                    ImGui::SliderInt("Discovery Interval (seconds)", &discovery_interval, 5, 120);

                    ImGui::EndTabItem();
                }

                if (ImGui::BeginTabItem("Storage")) {
                    ImGui::Text("Storage Settings");
                    ImGui::Separator();

                    static char storage_path[256] = ".";
                    ImGui::InputText("Storage Path", storage_path, IM_ARRAYSIZE(storage_path));
                    ImGui::SameLine();
                    if (ImGui::Button("Browse##Storage")) {
                    }

                    static int max_version_count = 10;
                    ImGui::SliderInt("Max Versions to Keep", &max_version_count, 1, 50);

                    static bool compress_files = true;
                    ImGui::Checkbox("Compress Files", &compress_files);

                    ImGui::EndTabItem();
                }

                ImGui::EndTabBar();
            }

            ImGui::Separator();
            if (ImGui::Button("Save", ImVec2(120, 0))) {
                m_show_settings = false;
            }
            ImGui::SameLine();
            if (ImGui::Button("Cancel", ImVec2(120, 0))) {
                m_show_settings = false;
            }
        }
        ImGui::End();
    }

    void UIManager::showConnectDialog() {
        ImGui::SetNextWindowSize(ImVec2(400, 200), ImGuiCond_FirstUseEver);
        if (ImGui::Begin("Connect to Peer", &m_show_connect)) {
            static char peer_address[128] = "127.0.0.1";
            ImGui::InputText("Peer Address", peer_address, IM_ARRAYSIZE(peer_address));

            static int peer_port = 12345;
            ImGui::InputInt("Peer Port", &peer_port);

            ImGui::Separator();
            if (ImGui::Button("Connect", ImVec2(120, 0))) {
                m_peer_manager->connect(peer_address, peer_port);
                m_show_connect = false;
            }
            ImGui::SameLine();
            if (ImGui::Button("Cancel", ImVec2(120, 0))) {
                m_show_connect = false;
            }
        }
        ImGui::End();
    }

    void UIManager::showNewProjectDialog() {
        ImGui::SetNextWindowSize(ImVec2(500, 300), ImGuiCond_FirstUseEver);
        if (ImGui::Begin("New Project", &m_show_new_project)) {
            static char project_name[128] = "";
            ImGui::InputText("Project Name", project_name, IM_ARRAYSIZE(project_name));

            static char project_code[64] = "";
            ImGui::InputText("Project Code", project_code, IM_ARRAYSIZE(project_code));

            static char description[512] = "";
            ImGui::InputTextMultiline("Description", description, IM_ARRAYSIZE(description), ImVec2(-1, 100));

            ImGui::Separator();
            if (ImGui::Button("Create", ImVec2(120, 0))) {
                Project project;
                project.name = project_name;
                project.code = project_code;
                project.description = description;
                project.created_at = std::chrono::system_clock::to_time_t(std::chrono::system_clock::now());
                project.updated_at = std::chrono::system_clock::to_time_t(std::chrono::system_clock::now());
                project.created_by = 1;

                m_database->createProject(project);

                m_show_new_project = false;
            }
            ImGui::SameLine();
            if (ImGui::Button("Cancel", ImVec2(120, 0))) {
                m_show_new_project = false;
            }
        }
        ImGui::End();
    }

    void UIManager::showNewAssetDialog() {
        ImGui::SetNextWindowSize(ImVec2(500, 400), ImGuiCond_FirstUseEver);
        if (ImGui::Begin("New Asset", &m_show_new_asset)) {
            static char asset_name[128] = "";
            ImGui::InputText("Asset Name", asset_name, IM_ARRAYSIZE(asset_name));

            static char asset_code[64] = "";
            ImGui::InputText("Asset Code", asset_code, IM_ARRAYSIZE(asset_code));

            static int asset_type = 0;
            const char* asset_types[] = {
                "Model", "Texture", "Material", "Rig", "Animation", "Scene",
                "Prop", "Character", "Environment", "Effect", "Audio", "Other"
            };
            ImGui::Combo("Asset Type", &asset_type, asset_types, IM_ARRAYSIZE(asset_types));

            static int project_id = 0;
            auto projects = m_database->getAllProjects();
            if (!projects.empty()) {
                std::vector<const char*> project_names;
                for (const auto& project : projects) {
                    project_names.push_back(project.name.c_str());
                }

                ImGui::Combo("Project", &project_id, project_names.data(), project_names.size());
            }
            else {
                ImGui::Text("No projects available. Create a project first.");
            }

            static char description[512] = "";
            ImGui::InputTextMultiline("Description", description, IM_ARRAYSIZE(description), ImVec2(-1, 100));

            static char tags[256] = "";
            ImGui::InputText("Tags (comma separated)", tags, IM_ARRAYSIZE(tags));

            ImGui::Separator();
            if (ImGui::Button("Create", ImVec2(120, 0))) {
                Asset asset;
                asset.name = asset_name;
                asset.code = asset_code;
                asset.description = description;
                asset.type = static_cast<AssetType>(asset_type);
                asset.project_id = project_id;
                asset.tags = tags;
                asset.created_at = std::chrono::system_clock::now();
                asset.updated_at = std::chrono::system_clock::now();
                asset.created_by = 1;
                asset.is_archived = false;

                m_database->createAsset(asset);

                m_show_new_asset = false;
            }
            ImGui::SameLine();
            if (ImGui::Button("Cancel", ImVec2(120, 0))) {
                m_show_new_asset = false;
            }
        }
        ImGui::End();
    }

    void UIManager::showImportDialog() {
        ImGui::SetNextWindowSize(ImVec2(500, 300), ImGuiCond_FirstUseEver);
        if (ImGui::Begin("Import", &m_show_import)) {
            static char import_path[512] = "";
            ImGui::InputText("File Path", import_path, IM_ARRAYSIZE(import_path));
            ImGui::SameLine();
            if (ImGui::Button("Browse")) {
            }

            static int import_type = 0;
            const char* import_types[] = {
                "Project", "Asset", "Version"
            };
            ImGui::Combo("Import Type", &import_type, import_types, IM_ARRAYSIZE(import_types));

            ImGui::Separator();
            if (ImGui::Button("Import", ImVec2(120, 0))) {
                m_database->importFromJson(import_path);
                m_show_import = false;
            }
            ImGui::SameLine();
            if (ImGui::Button("Cancel", ImVec2(120, 0))) {
                m_show_import = false;
            }
        }
        ImGui::End();
    }

    void UIManager::showExportDialog() {
        ImGui::SetNextWindowSize(ImVec2(500, 300), ImGuiCond_FirstUseEver);
        if (ImGui::Begin("Export", &m_show_export)) {
            static char export_path[512] = "";
            ImGui::InputText("File Path", export_path, IM_ARRAYSIZE(export_path));
            ImGui::SameLine();
            if (ImGui::Button("Browse")) {
            }

            static int export_type = 0;
            const char* export_types[] = {
                "All", "Project", "Asset", "Version"
            };
            ImGui::Combo("Export Type", &export_type, export_types, IM_ARRAYSIZE(export_types));

            static int selected_id = 0;
            if (export_type == 1) {
                auto projects = m_database->getAllProjects();
                if (!projects.empty()) {
                    std::vector<const char*> project_names;
                    for (const auto& project : projects) {
                        project_names.push_back(project.name.c_str());
                    }

                    ImGui::Combo("Project", &selected_id, project_names.data(), project_names.size());
                }
            }
            else if (export_type == 2) {
            }
            else if (export_type == 3) {
            }

            ImGui::Separator();
            if (ImGui::Button("Export", ImVec2(120, 0))) {
                m_database->exportToJson(export_path);
                m_show_export = false;
            }
            ImGui::SameLine();
            if (ImGui::Button("Cancel", ImVec2(120, 0))) {
                m_show_export = false;
            }
        }
        ImGui::End();
    }

} // namespace p2p
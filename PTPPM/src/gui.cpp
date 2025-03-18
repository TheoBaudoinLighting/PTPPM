#include "gui.h"
#include "config.h"
#include "server.h"
#include "client.h"
#include <imgui.h>
#include <backends/imgui_impl_glfw.h>
#include <backends/imgui_impl_opengl3.h>
#include <spdlog/spdlog.h>
#include <stdexcept>
#include <iostream>

void GUI::glfw_error_callback(int error, const char* description) {
    spdlog::error("GLFW Error {}: {}", error, description);
}

GUI::GUI(std::atomic<bool>& running, std::unique_ptr<TCPServer>& server) : running_(running), server_(server), window_(nullptr), server_port_config_(DEFAULT_TCP_PORT), server_running_(false) {
}

GUI::~GUI() {
    try {
        if (server_running_) {
            server_->stop();
            if (server_thread_.joinable()) {
                server_thread_.join();
            }
            server_running_ = false;
        }
        cleanup_imgui();
        if (window_) {
            glfwDestroyWindow(window_);
        }
        glfwTerminate();
        spdlog::info("Interface graphique nettoyée");
    }
    catch (const std::exception& e) {
        spdlog::error("Erreur lors du nettoyage de l'interface graphique: {}", e.what());
    }
}

void GUI::run() {
    try {
        if (!init_glfw()) {
            throw std::runtime_error("Échec de l'initialisation de GLFW");
        }
        if (!init_glad()) {
            throw std::runtime_error("Échec de l'initialisation de GLAD");
        }
        init_imgui();
        setup_dark_theme();
        while (!glfwWindowShouldClose(window_) && running_) {
            glfwPollEvents();
            ImGui_ImplOpenGL3_NewFrame();
            ImGui_ImplGlfw_NewFrame();
            ImGui::NewFrame();
            render_ui();
            ImGui::Render();
            int display_w, display_h;
            glfwGetFramebufferSize(window_, &display_w, &display_h);
            glViewport(0, 0, display_w, display_h);
            glClearColor(clear_color_[0], clear_color_[1], clear_color_[2], clear_color_[3]);
            glClear(GL_COLOR_BUFFER_BIT);
            ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
            glfwSwapBuffers(window_);
        }
        if (glfwWindowShouldClose(window_)) {
            running_ = false;
        }
        spdlog::info("Boucle de rendu terminée");
    }
    catch (const std::exception& e) {
        spdlog::critical("Erreur dans l'interface graphique: {}", e.what());
        running_ = false;
    }
}

bool GUI::init_glfw() {
    glfwSetErrorCallback(glfw_error_callback);
    if (!glfwInit()) {
        spdlog::error("Impossible d'initialiser GLFW");
        return false;
    }
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
    window_ = glfwCreateWindow(window_width_, window_height_, window_title_.c_str(), nullptr, nullptr);
    if (!window_) {
        spdlog::error("Impossible de créer une fenêtre GLFW");
        glfwTerminate();
        return false;
    }
    glfwMakeContextCurrent(window_);
    glfwSwapInterval(1);
    spdlog::info("GLFW initialisé");
    return true;
}

bool GUI::init_glad() {
    if (!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress)) {
        spdlog::error("Impossible d'initialiser GLAD");
        return false;
    }
    spdlog::info("GLAD initialisé");
    return true;
}

void GUI::init_imgui() {
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
    ImGui_ImplGlfw_InitForOpenGL(window_, true);
    ImGui_ImplOpenGL3_Init("#version 330");
    spdlog::info("ImGui initialisé");
}

void GUI::cleanup_imgui() {
    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();
    spdlog::info("ImGui nettoyé");
}

void GUI::render_ui() {
    if (ImGui::BeginMainMenuBar()) {
        if (ImGui::MenuItem("Serveur", NULL, show_server_tab_)) {
            show_server_tab_ = !show_server_tab_;
        }
        if (ImGui::MenuItem("Client", NULL, show_client_tab_)) {
            show_client_tab_ = !show_client_tab_;
        }
        if (ImGui::MenuItem("Options")) {
            ImGui::OpenPopup("OptionsPopup");
        }
        if (ImGui::MenuItem("Quitter")) {
            running_ = false;
        }
        if (ImGui::BeginPopup("OptionsPopup")) {
            ImGui::Checkbox("Afficher la démo ImGui", &show_demo_window_);
            ImGui::Checkbox("Afficher les métriques ImGui", &show_metrics_);
            ImGui::ColorEdit3("Couleur d'arrière-plan", clear_color_);
            ImGui::EndPopup();
        }
        ImGui::EndMainMenuBar();
    }
    if (show_server_tab_) {
        render_server_ui();
    }
    if (show_client_tab_) {
        render_client_ui();
    }
    if (show_demo_window_) {
        ImGui::ShowDemoWindow(&show_demo_window_);
    }
    if (show_metrics_) {
        ImGui::ShowMetricsWindow(&show_metrics_);
    }
}

void GUI::render_server_ui() {
    ImGui::Begin("Serveur TCP", &show_server_tab_, ImGuiWindowFlags_AlwaysAutoResize);
    ImGui::InputInt("Port d'écoute", &server_port_config_);
    if (server_port_config_ < 1) server_port_config_ = 1;
    if (server_port_config_ > 65535) server_port_config_ = 65535;
    if (server_running_) {
        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.0f, 1.0f, 0.0f, 1.0f));
        ImGui::Text("Serveur en cours d'exécution sur le port %d", server_->get_port());
        ImGui::PopStyleColor();
        if (ImGui::Button("Arrêter le serveur")) {
            server_->stop();
            if (server_thread_.joinable()) {
                server_thread_.join();
            }
            server_running_ = false;
            spdlog::info("Serveur arrêté par l'utilisateur");
        }
    }
    else {
        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.0f, 0.0f, 0.0f, 1.0f));
        ImGui::Text("Serveur arrêté");
        ImGui::PopStyleColor();
        if (ImGui::Button("Démarrer le serveur")) {
            if (server_thread_.joinable()) {
                server_thread_.join();
            }
            server_->stop();
            unsigned short port = static_cast<unsigned short>(server_port_config_);
            server_ = std::make_unique<TCPServer>(port);
            server_running_ = true;
            server_thread_ = std::thread([this]() {
                try {
                    server_->run(running_);
                }
                catch (const std::exception& e) {
                    spdlog::error("Erreur dans le thread du serveur: {}", e.what());
                    server_running_ = false;
                }
            });
            spdlog::info("Serveur démarré sur le port {}", server_port_config_);
        }
    }
    ImGui::Separator();
    if (ImGui::Button("Quitter l'application")) {
        running_ = false;
    }
    if (ImGui::CollapsingHeader("Logs de connexion", ImGuiTreeNodeFlags_DefaultOpen)) {
        auto logs = server_->get_connection_logs();
        if (logs.empty()) {
            ImGui::TextDisabled("Aucune connexion enregistrée");
        }
        else {
            ImGui::BeginChild("ServerLogs", ImVec2(0, 200), true);
            for (const auto& log : logs) {
                ImGui::TextUnformatted(log.c_str());
            }
            if (ImGui::GetScrollY() >= ImGui::GetScrollMaxY()) {
                ImGui::SetScrollHereY(1.0f);
            }
            ImGui::EndChild();
        }
    }
    ImGui::End();
}

void GUI::render_client_ui() {
    ImGui::Begin("Client TCP", &show_client_tab_);
    if (client_.is_connected()) {
        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.0f, 1.0f, 0.0f, 1.0f));
        ImGui::Text("Connecté à %s:%d", server_ip_, server_port_);
        ImGui::PopStyleColor();
        if (ImGui::Button("Déconnecter")) {
            client_.disconnect();
        }
        ImGui::Separator();
        ImGui::InputText("Message", message_to_send_, sizeof(message_to_send_));
        ImGui::SameLine();
        if (ImGui::Button("Envoyer") && strlen(message_to_send_) > 0) {
            if (client_.send_message(message_to_send_)) {
                message_to_send_[0] = '\0';
            }
        }
    }
    else {
        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.0f, 0.0f, 0.0f, 1.0f));
        ImGui::Text("Non connecté");
        ImGui::PopStyleColor();
        ImGui::InputText("Adresse IP du serveur", server_ip_, sizeof(server_ip_));
        ImGui::InputInt("Port du serveur", &server_port_);
        if (server_port_ < 1) server_port_ = 1;
        if (server_port_ > 65535) server_port_ = 65535;
        if (ImGui::Button("Connecter")) {
            client_.connect(server_ip_, static_cast<unsigned short>(server_port_));
        }
    }
    ImGui::Separator();
    ImGui::Text("Messages");
    auto messages = client_.get_received_messages();
    ImGui::BeginChild("ClientMessages", ImVec2(0, 200), true);
    for (const auto& msg : messages) {
        ImGui::TextUnformatted(msg.c_str());
    }
    if (ImGui::GetScrollY() >= ImGui::GetScrollMaxY()) {
        ImGui::SetScrollHereY(1.0f);
    }
    ImGui::EndChild();
    ImGui::End();
}

void GUI::setup_dark_theme() {
    ImGui::StyleColorsDark();
    ImGuiStyle& style = ImGui::GetStyle();
    style.WindowRounding = 6.0f;
    style.ChildRounding = 6.0f;
    style.FrameRounding = 3.0f;
    style.PopupRounding = 3.0f;
    style.ScrollbarRounding = 3.0f;
    style.GrabRounding = 3.0f;
    style.TabRounding = 3.0f;
    style.WindowPadding = ImVec2(10, 10);
    style.FramePadding = ImVec2(5, 5);
    style.ItemSpacing = ImVec2(6, 5);
    ImVec4* colors = style.Colors;
    colors[ImGuiCol_WindowBg] = ImVec4(0.10f, 0.10f, 0.10f, 1.00f);
    colors[ImGuiCol_Header] = ImVec4(0.26f, 0.59f, 0.98f, 0.31f);
    colors[ImGuiCol_HeaderHovered] = ImVec4(0.26f, 0.59f, 0.98f, 0.80f);
    colors[ImGuiCol_HeaderActive] = ImVec4(0.26f, 0.59f, 0.98f, 1.00f);
    colors[ImGuiCol_Button] = ImVec4(0.26f, 0.59f, 0.98f, 0.40f);
    colors[ImGuiCol_ButtonHovered] = ImVec4(0.26f, 0.59f, 0.98f, 1.00f);
    colors[ImGuiCol_ButtonActive] = ImVec4(0.06f, 0.53f, 0.98f, 1.00f);
}

#include "user_interface.h"
#include <imgui.h>
#include <backends/imgui_impl_glfw.h>
#include <backends/imgui_impl_opengl3.h>
#include <spdlog/spdlog.h>
#include <stdexcept>

void UserInterface::glfwErrorCallback(int error, const char* description) {
    spdlog::error("GLFW Error {}: {}", error, description);
}

UserInterface::UserInterface(int windowWidth, int windowHeight, const std::string& windowTitle)
    : window_(nullptr),
      windowWidth_(windowWidth),
      windowHeight_(windowHeight),
      windowTitle_(windowTitle) {
}

UserInterface::~UserInterface() {
    cleanup();
}

bool UserInterface::initialize() {
    try {
        if (!initGLFW()) {
            throw std::runtime_error("Échec de l'initialisation de GLFW");
        }
        
        if (!initGLAD()) {
            throw std::runtime_error("Échec de l'initialisation de GLAD");
        }
        
        initImGui();
        setupDarkTheme();
        
        return true;
    }
    catch (const std::exception& e) {
        spdlog::critical("Erreur lors de l'initialisation de l'interface: {}", e.what());
        return false;
    }
}

void UserInterface::run(std::atomic<bool>& running) {
    try {
        while (!glfwWindowShouldClose(window_) && running) {
            glfwPollEvents();
            
            ImGui_ImplOpenGL3_NewFrame();
            ImGui_ImplGlfw_NewFrame();
            ImGui::NewFrame();
            
            renderUI();
            
            ImGui::Render();
            
            int displayW, displayH;
            glfwGetFramebufferSize(window_, &displayW, &displayH);
            glViewport(0, 0, displayW, displayH);
            glClearColor(clearColor_[0], clearColor_[1], clearColor_[2], clearColor_[3]);
            glClear(GL_COLOR_BUFFER_BIT);
            
            ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
            glfwSwapBuffers(window_);
        }
        
        if (glfwWindowShouldClose(window_)) {
            running = false;
        }
        
        spdlog::info("Boucle de rendu terminée");
    }
    catch (const std::exception& e) {
        spdlog::critical("Erreur dans l'interface graphique: {}", e.what());
        running = false;
    }
}

void UserInterface::cleanup() {
    try {
        cleanupImGui();
        
        if (window_) {
            glfwDestroyWindow(window_);
            window_ = nullptr;
        }
        
        glfwTerminate();
        spdlog::info("Interface graphique nettoyée");
    }
    catch (const std::exception& e) {
        spdlog::error("Erreur lors du nettoyage de l'interface graphique: {}", e.what());
    }
}

bool UserInterface::initGLFW() {
    glfwSetErrorCallback(glfwErrorCallback);
    
    if (!glfwInit()) {
        spdlog::error("Impossible d'initialiser GLFW");
        return false;
    }
    
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
    
    window_ = glfwCreateWindow(windowWidth_, windowHeight_, windowTitle_.c_str(), nullptr, nullptr);
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

bool UserInterface::initGLAD() {
    if (!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress)) {
        spdlog::error("Impossible d'initialiser GLAD");
        return false;
    }
    
    spdlog::info("GLAD initialisé");
    return true;
}

void UserInterface::initImGui() {
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
    
    ImGui_ImplGlfw_InitForOpenGL(window_, true);
    ImGui_ImplOpenGL3_Init("#version 330");
    
    spdlog::info("ImGui initialisé");
}

void UserInterface::cleanupImGui() {
    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();
    
    spdlog::info("ImGui nettoyé");
}

void UserInterface::renderUI() {
    if (ImGui::BeginMainMenuBar()) {
        if (ImGui::MenuItem("Serveur", NULL, showServerTab_)) {
            showServerTab_ = !showServerTab_;
        }
        
        if (ImGui::MenuItem("Client", NULL, showClientTab_)) {
            showClientTab_ = !showClientTab_;
        }
        
        if (ImGui::MenuItem("Options")) {
            ImGui::OpenPopup("OptionsPopup");
        }
        
        if (ImGui::MenuItem("Quitter")) {
            glfwSetWindowShouldClose(window_, GLFW_TRUE);
        }
        
        ImGui::EndMainMenuBar();
    }
    
    if (showServerTab_) {
        renderServerUI();
    }
    
    if (showClientTab_) {
        renderClientUI();
    }
    
    if (showDemoWindow_) {
        ImGui::ShowDemoWindow(&showDemoWindow_);
    }
    
    if (showMetrics_) {
        ImGui::ShowMetricsWindow(&showMetrics_);
    }
}

void UserInterface::renderServerUI() {
    ImGui::Begin("Serveur TCP", &showServerTab_, ImGuiWindowFlags_AlwaysAutoResize);
    
    ImGui::InputInt("Port d'écoute", &serverPortConfig_);
    if (serverPortConfig_ < 1) serverPortConfig_ = 1;
    if (serverPortConfig_ > 65535) serverPortConfig_ = 65535;
    
    bool isServerRunning = serverStatusCallback_ ? serverStatusCallback_() : false;
    
    if (isServerRunning) {
        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.0f, 1.0f, 0.0f, 1.0f));
        ImGui::Text("Serveur en cours d'exécution sur le port %d", serverPortConfig_);
        ImGui::PopStyleColor();
        
        if (ImGui::Button("Arrêter le serveur")) {
            if (serverStopCallback_) {
                serverStopCallback_();
            }
            spdlog::info("Serveur arrêté par l'utilisateur");
        }
    }
    else {
        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.0f, 0.0f, 0.0f, 1.0f));
        ImGui::Text("Serveur arrêté");
        ImGui::PopStyleColor();
        
        if (ImGui::Button("Démarrer le serveur")) {
            if (serverStartCallback_) {
                unsigned short port = static_cast<unsigned short>(serverPortConfig_);
                serverStartCallback_(port);
            }
            spdlog::info("Serveur démarré sur le port {}", serverPortConfig_);
        }
    }
    
    ImGui::Separator();
    
    if (ImGui::Button("Quitter l'application")) {
        glfwSetWindowShouldClose(window_, GLFW_TRUE);
    }
    
    if (ImGui::CollapsingHeader("Logs de connexion", ImGuiTreeNodeFlags_DefaultOpen)) {
        std::vector<std::string> logs;
        if (serverLogsCallback_) {
            logs = serverLogsCallback_();
        }
        
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

void UserInterface::renderClientUI() {
    ImGui::Begin("Client TCP", &showClientTab_);
    
    bool isClientConnected = clientStatusCallback_ ? clientStatusCallback_() : false;
    
    if (isClientConnected) {
        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.0f, 1.0f, 0.0f, 1.0f));
        ImGui::Text("Connecté à %s:%d", serverIp_, serverPort_);
        ImGui::PopStyleColor();
        
        if (ImGui::Button("Déconnecter")) {
            if (clientDisconnectCallback_) {
                clientDisconnectCallback_();
            }
        }
        
        ImGui::Separator();
        ImGui::InputText("Message", messageToSend_, sizeof(messageToSend_));
        ImGui::SameLine();
        
        if (ImGui::Button("Envoyer") && strlen(messageToSend_) > 0) {
            if (clientSendCallback_ && clientSendCallback_(messageToSend_)) {
                messageToSend_[0] = '\0';
            }
        }
    }
    else {
        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.0f, 0.0f, 0.0f, 1.0f));
        ImGui::Text("Non connecté");
        ImGui::PopStyleColor();
        
        ImGui::InputText("Adresse IP du serveur", serverIp_, sizeof(serverIp_));
        ImGui::InputInt("Port du serveur", &serverPort_);
        
        if (serverPort_ < 1) serverPort_ = 1;
        if (serverPort_ > 65535) serverPort_ = 65535;
        
        if (ImGui::Button("Connecter")) {
            if (clientConnectCallback_) {
                clientConnectCallback_(serverIp_, static_cast<unsigned short>(serverPort_));
            }
        }
    }
    
    ImGui::Separator();
    ImGui::Text("Messages");
    
    std::vector<std::string> messages;
    if (clientMessagesCallback_) {
        messages = clientMessagesCallback_();
    }
    
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

void UserInterface::setupDarkTheme() {
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

void UserInterface::setServerStartCallback(const std::function<bool(unsigned short)>& callback) {
    serverStartCallback_ = callback;
}

void UserInterface::setServerStopCallback(const std::function<void()>& callback) {
    serverStopCallback_ = callback;
}

void UserInterface::setServerStatusCallback(const std::function<bool()>& callback) {
    serverStatusCallback_ = callback;
}

void UserInterface::setServerLogsCallback(const std::function<std::vector<std::string>()>& callback) {
    serverLogsCallback_ = callback;
}

void UserInterface::setClientConnectCallback(const std::function<bool(const std::string&, unsigned short)>& callback) {
    clientConnectCallback_ = callback;
}

void UserInterface::setClientDisconnectCallback(const std::function<void()>& callback) {
    clientDisconnectCallback_ = callback;
}

void UserInterface::setClientSendCallback(const std::function<bool(const std::string&)>& callback) {
    clientSendCallback_ = callback;
}

void UserInterface::setClientStatusCallback(const std::function<bool()>& callback) {
    clientStatusCallback_ = callback;
}

void UserInterface::setClientMessagesCallback(const std::function<std::vector<std::string>()>& callback) {
    clientMessagesCallback_ = callback;
}
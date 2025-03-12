#include "application.h"
#include <spdlog/spdlog.h>
#include <imgui.h>
#include <imgui_impl_glfw.h>
#include <imgui_impl_opengl3.h>
#include <glad/glad.h>
#include <filesystem>
#include <chrono>

namespace p2p {

    Application::Application()
        : m_window(nullptr),
        m_width(0),
        m_height(0),
        m_is_running(false),
        m_is_initialized(false) {
    }

    Application::~Application() {
        if (m_is_initialized) {
            shutdown();
        }
    }

    bool Application::initialize(int width, int height, const std::string& title) {
        m_width = width;
        m_height = height;
        m_title = title;

        std::string app_data_path;
#ifdef _WIN32
        const char* appdata = std::getenv("APPDATA");
        if (appdata) {
            app_data_path = appdata;
            app_data_path += "\\P2PPipelineManager";
        } else {
            app_data_path = ".\\P2PPipelineManager";
        }
#else
        const char* home = std::getenv("HOME");
        if (home) {
            app_data_path = home;
            app_data_path += "/.p2ppipelinemanager";
        } else {
            app_data_path = "./p2ppipelinemanager";
        }
#endif

        try {
            std::filesystem::create_directories(app_data_path);
            m_config_path = app_data_path + "/config.json";
        }
        catch (const std::exception& e) {
            spdlog::error("Failed to create config directory: {}", e.what());
            return false;
        }

        if (!initializeGLFW()) {
            spdlog::error("Failed to initialize GLFW");
            return false;
        }

        if (!initializeGLAD()) {
            spdlog::error("Failed to initialize GLAD");
            return false;
        }

        if (!initializeImGui()) {
            spdlog::error("Failed to initialize ImGui");
            return false;
        }

        if (!initializeComponents()) {
            spdlog::error("Failed to initialize components");
            return false;
        }

        m_is_initialized = true;
        spdlog::info("Application initialized successfully");
        return true;
    }

    void Application::run() {
        if (!m_is_initialized) {
            spdlog::error("Cannot run uninitialized application");
            return;
        }

        m_is_running = true;

        auto last_frame_time = std::chrono::high_resolution_clock::now();

        while (m_is_running && !glfwWindowShouldClose(m_window)) {
            auto current_frame_time = std::chrono::high_resolution_clock::now();
            float delta_time = std::chrono::duration<float, std::chrono::seconds::period>(current_frame_time - last_frame_time).count();
            last_frame_time = current_frame_time;

            glfwPollEvents();
            processInput();

            update(delta_time);
            render();
        }
    }

    void Application::shutdown() {
        if (!m_is_initialized) {
            return;
        }

        spdlog::info("Shutting down version manager");
        if (m_version_manager) {
            m_version_manager->shutdown();
        }

        spdlog::info("Shutting down database");
        if (m_database) {
            m_database->shutdown();
        }

        spdlog::info("Shutting down peer manager");
        if (m_peer_manager) {
            m_peer_manager->shutdown();
        }

        spdlog::info("Shutting down UI manager");
        if (m_ui_manager) {
            m_ui_manager->shutdown();
        }

        spdlog::info("Shutting down renderer");
        if (m_renderer) {
            m_renderer->shutdown();
        }

        ImGui_ImplOpenGL3_Shutdown();
        ImGui_ImplGlfw_Shutdown();
        ImGui::DestroyContext();

        if (m_window) {
            glfwDestroyWindow(m_window);
            m_window = nullptr;
        }
        glfwTerminate();

        m_is_initialized = false;
        spdlog::info("Application shutdown complete");
    }

    bool Application::initializeGLFW() {
        if (!glfwInit()) {
            return false;
        }

        glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 4);
        glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 5);
        glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

        m_window = glfwCreateWindow(m_width, m_height, m_title.c_str(), nullptr, nullptr);
        if (!m_window) {
            glfwTerminate();
            return false;
        }

        glfwSetWindowUserPointer(m_window, this);
        glfwSetFramebufferSizeCallback(m_window, frameBufferResizeCallback);
        glfwSetKeyCallback(m_window, keyCallback);
        glfwSetMouseButtonCallback(m_window, mouseButtonCallback);

        glfwMakeContextCurrent(m_window);
        glfwSwapInterval(1);

        return true;
    }

    bool Application::initializeGLAD() {
        if (!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress)) {
            return false;
        }

        return true;
    }

    bool Application::initializeImGui() {
        IMGUI_CHECKVERSION();
        ImGui::CreateContext();
        ImGuiIO& io = ImGui::GetIO();
        (void)io;

        io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;
        io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;

        ImGui_ImplGlfw_InitForOpenGL(m_window, true);
        ImGui_ImplOpenGL3_Init("#version 450");
        ImGui::StyleColorsDark();

        return true;
    }

    bool Application::initializeComponents() {
        try {
            m_renderer = std::make_unique<Renderer>();
            if (!m_renderer->initialize()) {
                spdlog::error("Failed to initialize renderer");
                return false;
            }

            std::string db_path = std::filesystem::path(m_config_path).parent_path().string() + "/pipeline.db";
            m_database = std::make_unique<Database>();
            if (!m_database->initialize(db_path)) {
                spdlog::error("Failed to initialize database");
                return false;
            }

            std::string repo_path = std::filesystem::path(m_config_path).parent_path().string() + "/repo";
            m_version_manager = std::make_unique<VersionManager>(m_database.get());
            if (!m_version_manager->initialize(repo_path)) {
                spdlog::error("Failed to initialize version manager");
                return false;
            }

            m_peer_manager = std::make_unique<PeerManager>();
            if (!m_peer_manager->initialize(12345, "User")) {
                spdlog::error("Failed to initialize peer manager");
                return false;
            }

            m_ui_manager = std::make_unique<UIManager>(
                m_window,
                m_database.get(),
                m_peer_manager.get(),
                m_version_manager.get()
            );
            if (!m_ui_manager->initialize()) {
                spdlog::error("Failed to initialize UI manager");
                return false;
            }

            return true;
        }
        catch (const std::exception& e) {
            spdlog::error("Exception during component initialization: {}", e.what());
            return false;
        }
    }

    void Application::frameBufferResizeCallback(GLFWwindow* window, int width, int height) {
        auto app = static_cast<Application*>(glfwGetWindowUserPointer(window));
        if (app) {
            app->m_width = width;
            app->m_height = height;
            if (app->m_renderer) {
                app->m_renderer->resize(width, height);
            }
            if (app->m_ui_manager) {
                app->m_ui_manager->handleResize(width, height);
            }
        }
    }

    void Application::keyCallback(GLFWwindow* window, int key, int /*scancode*/, int action, int /*mods*/) {
        auto app = static_cast<Application*>(glfwGetWindowUserPointer(window));
    }

    void Application::mouseButtonCallback(GLFWwindow* window, int /*button*/, int /*action*/, int /*mods*/) {
        auto app = static_cast<Application*>(glfwGetWindowUserPointer(window));
    }

    void Application::update(float delta_time) {
    }

    void Application::render() {
        m_renderer->beginFrame();
        m_ui_manager->render();
        m_renderer->endFrame();
        glfwSwapBuffers(m_window);
    }

    void Application::processInput() {
        if (glfwGetKey(m_window, GLFW_KEY_ESCAPE) == GLFW_PRESS) {
            m_is_running = false;
        }
    }

} // namespace p2p
#pragma once
#include "server.h"
#include "client.h"
#include "config.h"
#include <glad/glad.h>
#include <GLFW/glfw3.h>
#include <atomic>
#include <memory>
#include <string>
#include <vector>
#include <thread>

class GUI {
public:
    GUI(std::atomic<bool>& running, std::unique_ptr<TCPServer>& server);
    ~GUI();
    void run();
private:
    bool init_glfw();
    bool init_glad();
    void init_imgui();
    void cleanup_imgui();
    void render_ui();
    void render_server_ui();
    void render_client_ui();
    void setup_dark_theme();
    static void glfw_error_callback(int error, const char* description);
    std::atomic<bool>& running_;
    std::unique_ptr<TCPServer>& server_;
    TCPClient client_;
    GLFWwindow* window_;
    int window_width_ = DEFAULT_WINDOW_WIDTH;
    int window_height_ = DEFAULT_WINDOW_HEIGHT;
    std::string window_title_ = DEFAULT_WINDOW_TITLE;
    bool show_demo_window_ = false;
    bool show_metrics_ = false;
    float clear_color_[4] = {0.2f, 0.3f, 0.3f, 1.0f};
    char server_ip_[128] = "127.0.0.1";
    int server_port_ = DEFAULT_TCP_PORT;
    char message_to_send_[1024] = "";
    bool show_server_tab_ = true;
    bool show_client_tab_ = true;
    int server_port_config_ = DEFAULT_TCP_PORT;
    bool server_running_ = false;
    std::thread server_thread_;
};

#pragma once
#include "iuser_interface.h"
#include <glad/glad.h>
#include <GLFW/glfw3.h>
#include <functional>
#include <atomic>
#include <string>
#include <vector>

class UserInterface : public IUserInterface {
public:
    UserInterface(int windowWidth, int windowHeight, const std::string& windowTitle);
    ~UserInterface() override;
    
    bool initialize() override;
    void run(std::atomic<bool>& running) override;
    void cleanup() override;
    
    void setServerStartCallback(const std::function<bool(unsigned short)>& callback) override;
    void setServerStopCallback(const std::function<void()>& callback) override;
    void setServerStatusCallback(const std::function<bool()>& callback) override;
    void setServerLogsCallback(const std::function<std::vector<std::string>()>& callback) override;
    
    void setClientConnectCallback(const std::function<bool(const std::string&, unsigned short)>& callback) override;
    void setClientDisconnectCallback(const std::function<void()>& callback) override;
    void setClientSendCallback(const std::function<bool(const std::string&)>& callback) override;
    void setClientStatusCallback(const std::function<bool()>& callback) override;
    void setClientMessagesCallback(const std::function<std::vector<std::string>()>& callback) override;
    
private:
    bool initGLFW();
    bool initGLAD();
    void initImGui();
    void cleanupImGui();
    void renderUI();
    void renderServerUI();
    void renderClientUI();
    void setupDarkTheme();
    static void glfwErrorCallback(int error, const char* description);
    
    GLFWwindow* window_;
    int windowWidth_;
    int windowHeight_;
    std::string windowTitle_;
    bool showDemoWindow_ = false;
    bool showMetrics_ = false;
    float clearColor_[4] = {0.2f, 0.3f, 0.3f, 1.0f};
    char serverIp_[128] = "127.0.0.1";
    int serverPort_ = 8080;
    char messageToSend_[1024] = "";
    bool showServerTab_ = true;
    bool showClientTab_ = true;
    int serverPortConfig_ = 8080;
    
    std::function<bool(unsigned short)> serverStartCallback_;
    std::function<void()> serverStopCallback_;
    std::function<bool()> serverStatusCallback_;
    std::function<std::vector<std::string>()> serverLogsCallback_;
    std::function<bool(const std::string&, unsigned short)> clientConnectCallback_;
    std::function<void()> clientDisconnectCallback_;
    std::function<bool(const std::string&)> clientSendCallback_;
    std::function<bool()> clientStatusCallback_;
    std::function<std::vector<std::string>()> clientMessagesCallback_;
};
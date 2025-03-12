#include <glad/glad.h>
#include "renderer/renderer.h"
#include <spdlog/spdlog.h>

namespace p2p {

    Renderer::Renderer()
        : m_vao(0),
        m_vbo(0),
        m_ebo(0),
        m_shader_program(0),
        m_viewport_width(0),
        m_viewport_height(0) {
    }

    Renderer::~Renderer() {
        shutdown();
    }

    bool Renderer::initialize() {
        if (!gladLoadGL()) {
            spdlog::error("GLAD n'est pas initialisé. Assurez-vous que gladLoadGL() a été appelé avant d'initialiser le renderer");
            return false;
        }
        
        if (!setupShaders()) {
            spdlog::error("Failed to setup shaders");
            return false;
        }

        if (!setupBuffers()) {
            spdlog::error("Failed to setup buffers");
            return false;
        }

        glEnable(GL_DEPTH_TEST);
        glEnable(GL_CULL_FACE);
        glCullFace(GL_BACK);

        spdlog::info("Renderer initialized successfully");
        return true;
    }

    void Renderer::shutdown() {
        if (m_vao) {
            glDeleteVertexArrays(1, &m_vao);
            m_vao = 0;
        }

        if (m_vbo) {
            glDeleteBuffers(1, &m_vbo);
            m_vbo = 0;
        }

        if (m_ebo) {
            glDeleteBuffers(1, &m_ebo);
            m_ebo = 0;
        }

        if (m_shader_program) {
            glDeleteProgram(m_shader_program);
            m_shader_program = 0;
        }
    }

    void Renderer::beginFrame() {
        glClearColor(0.1f, 0.1f, 0.1f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    }

    void Renderer::endFrame() {
    }

    void Renderer::resize(int width, int height) {
        m_viewport_width = width;
        m_viewport_height = height;
        glViewport(0, 0, width, height);
    }

    bool Renderer::setupShaders() {
        const char* vertexShaderSource = R"(
        #version 450 core
        layout (location = 0) in vec3 aPos;

        void main() {
            gl_Position = vec4(aPos, 1.0);
        }
    )";

        const char* fragmentShaderSource = R"(
        #version 450 core
        out vec4 FragColor;

        void main() {
            FragColor = vec4(1.0, 1.0, 1.0, 1.0);
        }
    )";

        unsigned int vertexShader = glCreateShader(GL_VERTEX_SHADER);
        glShaderSource(vertexShader, 1, &vertexShaderSource, NULL);
        glCompileShader(vertexShader);

        int success;
        char infoLog[512];
        glGetShaderiv(vertexShader, GL_COMPILE_STATUS, &success);
        if (!success) {
            glGetShaderInfoLog(vertexShader, 512, NULL, infoLog);
            spdlog::error("Vertex shader compilation failed: {}", infoLog);
            return false;
        }

        unsigned int fragmentShader = glCreateShader(GL_FRAGMENT_SHADER);
        glShaderSource(fragmentShader, 1, &fragmentShaderSource, NULL);
        glCompileShader(fragmentShader);

        glGetShaderiv(fragmentShader, GL_COMPILE_STATUS, &success);
        if (!success) {
            glGetShaderInfoLog(fragmentShader, 512, NULL, infoLog);
            spdlog::error("Fragment shader compilation failed: {}", infoLog);
            return false;
        }

        m_shader_program = glCreateProgram();
        glAttachShader(m_shader_program, vertexShader);
        glAttachShader(m_shader_program, fragmentShader);
        glLinkProgram(m_shader_program);

        glGetProgramiv(m_shader_program, GL_LINK_STATUS, &success);
        if (!success) {
            glGetProgramInfoLog(m_shader_program, 512, NULL, infoLog);
            spdlog::error("Shader program linking failed: {}", infoLog);
            return false;
        }

        glDeleteShader(vertexShader);
        glDeleteShader(fragmentShader);

        return true;
    }

    bool Renderer::setupBuffers() {
        float vertices[] = {
            -0.5f, -0.5f, 0.0f,
             0.5f, -0.5f, 0.0f,
             0.5f,  0.5f, 0.0f,
            -0.5f,  0.5f, 0.0f
        };

        unsigned int indices[] = {
            0, 1, 2,
            2, 3, 0
        };

        glGenVertexArrays(1, &m_vao);
        glBindVertexArray(m_vao);

        glGenBuffers(1, &m_vbo);
        glBindBuffer(GL_ARRAY_BUFFER, m_vbo);
        glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW);

        glGenBuffers(1, &m_ebo);
        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, m_ebo);
        glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(indices), indices, GL_STATIC_DRAW);

        glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), (void*)0);
        glEnableVertexAttribArray(0);

        glBindBuffer(GL_ARRAY_BUFFER, 0);
        glBindVertexArray(0);

        return true;
    }

} // namespace p2p
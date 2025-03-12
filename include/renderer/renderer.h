#pragma once

#include <glad/glad.h>
#include <string>

namespace p2p {

    class Renderer {
    public:
        Renderer();
        ~Renderer();

        bool initialize();
        void shutdown();
        void render();
        void resize(int width, int height);
        void beginFrame();
        void endFrame();

    private:
        unsigned int m_vao;
        unsigned int m_vbo;
        unsigned int m_ebo;
        unsigned int m_shader_program;
        
        int m_viewport_width;
        int m_viewport_height;

        const char* vertexShaderSource = R"(
            #version 330 core
            layout (location = 0) in vec3 aPos;
            void main() {
                gl_Position = vec4(aPos.x, aPos.y, aPos.z, 1.0);
            }
        )";
        
        const char* fragmentShaderSource = R"(
            #version 330 core
            out vec4 FragColor;
            void main() {
                FragColor = vec4(1.0f, 0.5f, 0.2f, 1.0f);
            }
        )";

        bool setupShaders();
        bool setupBuffers();
    };

} // namespace p2p
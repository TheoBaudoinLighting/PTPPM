#pragma once

#include <string>
#include <imgui.h>

namespace p2p {

    class Panel {
    public:
        Panel(const std::string& title) : m_title(title), m_is_visible(true) {}
        virtual ~Panel() = default;

        virtual void render() = 0;

        void show() { m_is_visible = true; }
        void hide() { m_is_visible = false; }
        bool isVisible() const { return m_is_visible; }

        const std::string& getTitle() const { return m_title; }

    protected:
        std::string m_title;
        bool m_is_visible;
    };

} // namespace p2p
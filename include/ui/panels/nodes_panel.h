#pragma once

#include "ui/panels/panel.h"

namespace p2p {
    class NodesPanel : public Panel {
    public:
        NodesPanel() : Panel("Nodes") {}
        void render() override {}
    };
}
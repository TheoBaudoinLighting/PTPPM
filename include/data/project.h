#pragma once
#include <string>
#include <ctime>

namespace p2p {
    struct Project {
        int id = 0;
        std::string name;
        std::string code;
        std::string description;
        time_t created_at = 0;
        time_t updated_at = 0;
        int created_by = 0;
    };
}
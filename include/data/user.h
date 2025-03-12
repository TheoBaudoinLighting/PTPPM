#pragma once

#include <string>
#include <ctime>

namespace p2p {

    struct User {
        int id = 0;
        std::string username;
        std::string display_name;
        std::string email;
        std::string avatar_path;
        time_t created_at = 0;
        time_t last_login = 0;
        
        bool operator==(const User& other) const {
            return id == other.id;
        }
    };

} // namespace p2p 
#pragma once

#include <string>
#include <ctime>

namespace p2p {

    struct Task {
        int id = 0;
        int asset_id = 0;
        int user_id = 0;
        std::string title;
        std::string description;
        std::string status;
        std::string priority;
        time_t created_at = 0;
        time_t updated_at = 0;
        time_t due_date = 0;
        
        bool operator==(const Task& other) const {
            return id == other.id;
        }
    };

} // namespace p2p 
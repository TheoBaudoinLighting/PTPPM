// include/data/version.h
#pragma once

#include <string>
#include <chrono>

namespace p2p {

    struct Version {
        int id;
        int asset_id;
        int version_number;
        std::string description;
        std::string path;
        std::string hash;
        int created_by;
        std::chrono::system_clock::time_point created_at;
        std::string status;
        std::string review_notes;
        int reviewed_by;
        std::chrono::system_clock::time_point reviewed_at;
        std::string metadata;
    };

} // namespace p2p
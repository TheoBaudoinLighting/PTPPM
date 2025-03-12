// include/data/asset.h
#pragma once

#include <string>
#include <vector>
#include <chrono>

namespace p2p {

    enum class AssetType {
        MODEL,
        TEXTURE,
        MATERIAL,
        RIG,
        ANIMATION,
        SCENE,
        PROP,
        CHARACTER,
        ENVIRONMENT,
        EFFECT,
        AUDIO,
        OTHER
    };

    struct Asset {
        int id;
        std::string name;
        std::string code;
        std::string description;
        AssetType type;
        int project_id;
        std::string path;
        std::string preview_url;
        std::string metadata;
        int created_by;
        std::chrono::system_clock::time_point created_at;
        std::chrono::system_clock::time_point updated_at;
        bool is_archived;
        std::string tags;
        int current_version_id;
    };

    std::string assetTypeToString(AssetType type);
    AssetType assetTypeFromString(const std::string& type_str);

} // namespace p2p
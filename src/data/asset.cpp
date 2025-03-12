#include "data/asset.h"

namespace p2p {

    std::string assetTypeToString(AssetType type) {
        switch (type) {
        case AssetType::MODEL:
            return "Model";
        case AssetType::TEXTURE:
            return "Texture";
        case AssetType::MATERIAL:
            return "Material";
        case AssetType::RIG:
            return "Rig";
        case AssetType::ANIMATION:
            return "Animation";
        case AssetType::SCENE:
            return "Scene";
        case AssetType::PROP:
            return "Prop";
        case AssetType::CHARACTER:
            return "Character";
        case AssetType::ENVIRONMENT:
            return "Environment";
        case AssetType::EFFECT:
            return "Effect";
        case AssetType::AUDIO:
            return "Audio";
        case AssetType::OTHER:
            return "Other";
        default:
            return "Unknown";
        }
    }

    AssetType assetTypeFromString(const std::string& type_str) {
        if (type_str == "Model") {
            return AssetType::MODEL;
        }
        else if (type_str == "Texture") {
            return AssetType::TEXTURE;
        }
        else if (type_str == "Material") {
            return AssetType::MATERIAL;
        }
        else if (type_str == "Rig") {
            return AssetType::RIG;
        }
        else if (type_str == "Animation") {
            return AssetType::ANIMATION;
        }
        else if (type_str == "Scene") {
            return AssetType::SCENE;
        }
        else if (type_str == "Prop") {
            return AssetType::PROP;
        }
        else if (type_str == "Character") {
            return AssetType::CHARACTER;
        }
        else if (type_str == "Environment") {
            return AssetType::ENVIRONMENT;
        }
        else if (type_str == "Effect") {
            return AssetType::EFFECT;
        }
        else if (type_str == "Audio") {
            return AssetType::AUDIO;
        }
        else {
            return AssetType::OTHER;
        }
    }

} // namespace p2p
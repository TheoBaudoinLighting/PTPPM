#pragma once

#include <string>
#include <vector>
#include <functional>
#include <mutex>

#include "data/database.h"
#include "data/version.h"
#include "data/asset.h"

namespace p2p {

    class VersionManager {
    public:
        VersionManager(Database* database);
        ~VersionManager();

        bool initialize(const std::string& repo_path);
        void shutdown();

        int createVersion(int asset_id, const std::string& file_path, const std::string& description);
        bool checkoutVersion(int version_id, const std::string& output_path);
        bool compareVersions(int version_id1, int version_id2, std::string& diff_output);

        std::string getVersionFilePath(int version_id);
        bool exportVersion(int version_id, const std::string& output_path);

        std::string getVersionHash(int version_id);
        std::string generateFileHash(const std::string& file_path);

        std::vector<Version> getVersionHistory(int asset_id);

        bool generatePreview(int asset_id, int version_id, const std::string& output_path);

        using VersionCreatedCallback = std::function<void(int asset_id, int version_id)>;
        void setVersionCreatedCallback(VersionCreatedCallback callback);

    private:
        Database* m_database;
        std::string m_repo_path;
        std::mutex m_cache_mutex;
        VersionCreatedCallback m_version_created_callback;

        std::string getAssetRepoPath(int asset_id);
        std::string constructVersionFilePath(int asset_id, int version_number);
        bool copyFileToRepo(const std::string& source_path, const std::string& target_path);
        int getNextVersionNumber(int asset_id);
    };

} // namespace p2p
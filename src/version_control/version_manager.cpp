#include "version_control/version_manager.h"
#include <spdlog/spdlog.h>
#include <filesystem>
#include <fstream>
#include <cstring>
#include "utils/sha256.h"

namespace p2p {

    VersionManager::VersionManager(Database* database)
        : m_database(database) {
    }

    VersionManager::~VersionManager() {
        shutdown();
    }

    bool VersionManager::initialize(const std::string& repo_path) {
        try {
            m_repo_path = repo_path;
            std::filesystem::create_directories(repo_path);
            spdlog::info("Version manager initialized with repository path: {}", repo_path);
            return true;
        }
        catch (const std::exception& e) {
            spdlog::error("Failed to initialize version manager: {}", e.what());
            return false;
        }
    }

    void VersionManager::shutdown() {
        spdlog::info("Version manager shutdown");
    }

    int VersionManager::createVersion(int asset_id, const std::string& file_path, const std::string& description) {
        try {
            int version_number = getNextVersionNumber(asset_id);
            std::string asset_repo_path = getAssetRepoPath(asset_id);
            std::filesystem::create_directories(asset_repo_path);
            std::string version_path = constructVersionFilePath(asset_id, version_number);

            if (!copyFileToRepo(file_path, version_path)) {
                spdlog::error("Failed to copy file to repository");
                return -1;
            }

            std::string hash = generateFileHash(version_path);
            Version version;
            version.asset_id = asset_id;
            version.version_number = version_number;
            version.description = description;
            version.path = version_path;
            version.hash = hash;
            version.created_by = 1;
            version.created_at = std::chrono::system_clock::now();
            version.status = "In Progress";

            int version_id = m_database->createVersion(version);
            Asset asset = m_database->getAsset(asset_id);
            asset.current_version_id = version_id;
            asset.updated_at = std::chrono::system_clock::now();
            m_database->updateAsset(asset);
            generatePreview(asset_id, version_id, asset_repo_path + "/preview.png");

            if (m_version_created_callback) {
                m_version_created_callback(asset_id, version_id);
            }

            spdlog::info("Created version {} for asset {}", version_number, asset_id);
            return version_id;
        }
        catch (const std::exception& e) {
            spdlog::error("Failed to create version: {}", e.what());
            return -1;
        }
    }

    bool VersionManager::checkoutVersion(int version_id, const std::string& output_path) {
        try {
            Version version = m_database->getVersion(version_id);
            if (!copyFileToRepo(version.path, output_path)) {
                spdlog::error("Failed to copy version file to output path");
                return false;
            }

            spdlog::info("Checked out version {} to {}", version_id, output_path);
            return true;
        }
        catch (const std::exception& e) {
            spdlog::error("Failed to checkout version: {}", e.what());
            return false;
        }
    }

    bool VersionManager::compareVersions(int version_id1, int version_id2, std::string& diff_output) {
        try {
            Version version1 = m_database->getVersion(version_id1);
            Version version2 = m_database->getVersion(version_id2);
            std::filesystem::path path1(version1.path);
            std::filesystem::path path2(version2.path);
            std::uintmax_t size1 = std::filesystem::file_size(path1);
            std::uintmax_t size2 = std::filesystem::file_size(path2);

            diff_output = "Version " + std::to_string(version1.version_number) + " size: " + std::to_string(size1) + " bytes\n";
            diff_output += "Version " + std::to_string(version2.version_number) + " size: " + std::to_string(size2) + " bytes\n";

            if (size1 > size2) {
                diff_output += "Version " + std::to_string(version1.version_number) + " is " + std::to_string(size1 - size2) + " bytes larger";
            }
            else if (size2 > size1) {
                diff_output += "Version " + std::to_string(version2.version_number) + " is " + std::to_string(size2 - size1) + " bytes larger";
            }
            else {
                diff_output += "Both versions have the same size";
            }

            spdlog::info("Compared versions {} and {}", version_id1, version_id2);
            return true;
        }
        catch (const std::exception& e) {
            spdlog::error("Failed to compare versions: {}", e.what());
            diff_output = "Error: " + std::string(e.what());
            return false;
        }
    }

    std::string VersionManager::getVersionFilePath(int version_id) {
        try {
            Version version = m_database->getVersion(version_id);
            return version.path;
        }
        catch (const std::exception& e) {
            spdlog::error("Failed to get version file path: {}", e.what());
            return "";
        }
    }

    bool VersionManager::exportVersion(int version_id, const std::string& output_path) {
        try {
            Version version = m_database->getVersion(version_id);
            if (!copyFileToRepo(version.path, output_path)) {
                spdlog::error("Failed to copy version file to output path");
                return false;
            }

            spdlog::info("Exported version {} to {}", version_id, output_path);
            return true;
        }
        catch (const std::exception& e) {
            spdlog::error("Failed to export version: {}", e.what());
            return false;
        }
    }

    std::string VersionManager::getVersionHash(int version_id) {
        try {
            Version version = m_database->getVersion(version_id);
            return version.hash;
        }
        catch (const std::exception& e) {
            spdlog::error("Failed to get version hash: {}", e.what());
            return "";
        }
    }

    std::string VersionManager::generateFileHash(const std::string& file_path) {
        try {
            return SHA256::hashFile(file_path);
        }
        catch (const std::exception& e) {
            spdlog::error("Failed to generate file hash: {}", e.what());
            return "";
        }
    }

    std::vector<Version> VersionManager::getVersionHistory(int asset_id) {
        try {
            return m_database->getVersionsByAsset(asset_id);
        }
        catch (const std::exception& e) {
            spdlog::error("Failed to get version history: {}", e.what());
            return {};
        }
    }

    bool VersionManager::generatePreview(int asset_id, int version_id, const std::string& output_path) {
        try {
            std::ofstream file(output_path, std::ios::binary);
            if (!file.is_open()) {
                spdlog::error("Failed to open preview file for writing: {}", output_path);
                return false;
            }

            const char png_header[] = {
                static_cast<char>(0x89), 'P', 'N', 'G', '\r', '\n', 0x1A, '\n'
            };
            file.write(png_header, sizeof(png_header));
            file.close();

            spdlog::info("Generated preview for asset {} version {}", asset_id, version_id);
            return true;
        }
        catch (const std::exception& e) {
            spdlog::error("Failed to generate preview: {}", e.what());
            return false;
        }
    }

    void VersionManager::setVersionCreatedCallback(VersionCreatedCallback callback) {
        m_version_created_callback = callback;
    }

    std::string VersionManager::getAssetRepoPath(int asset_id) {
        return m_repo_path + "/asset_" + std::to_string(asset_id);
    }

    std::string VersionManager::constructVersionFilePath(int asset_id, int version_number) {
        return getAssetRepoPath(asset_id) + "/v" + std::to_string(version_number);
    }

    bool VersionManager::copyFileToRepo(const std::string& source_path, const std::string& target_path) {
        try {
            std::filesystem::path target(target_path);
            std::filesystem::create_directories(target.parent_path());
            std::filesystem::copy_file(
                source_path,
                target_path,
                std::filesystem::copy_options::overwrite_existing
            );

            return true;
        }
        catch (const std::exception& e) {
            spdlog::error("Failed to copy file to repository: {}", e.what());
            return false;
        }
    }

    int VersionManager::getNextVersionNumber(int asset_id) {
        try {
            auto versions = m_database->getVersionsByAsset(asset_id);
            if (versions.empty()) {
                return 1;
            }

            int highest_version = 0;
            for (const auto& version : versions) {
                highest_version = std::max(highest_version, version.version_number);
            }

            return highest_version + 1;
        }
        catch (const std::exception& e) {
            spdlog::error("Failed to get next version number: {}", e.what());
            return 1;
        }
    }

} // namespace p2p
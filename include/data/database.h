#pragma once

#include <string>
#include <vector>
#include <memory>
#include <mutex>
#include <functional>
#include <sqlite3.h>

#include "data/asset.h"
#include "data/version.h"
#include "data/task.h"
#include "data/project.h"
#include "data/user.h"

namespace p2p {

    class Database {
    public:
        Database();
        ~Database();

        bool initialize(const std::string& db_path);
        void shutdown();

        int createProject(const Project& project);
        bool updateProject(const Project& project);
        bool deleteProject(int project_id);
        Project getProject(int project_id);
        std::vector<Project> getAllProjects();

        int createAsset(const Asset& asset);
        bool updateAsset(const Asset& asset);
        bool deleteAsset(int asset_id);
        Asset getAsset(int asset_id);
        std::vector<Asset> getAssetsByProject(int project_id);
        std::vector<Asset> getAllAssets();

        int createVersion(const Version& version);
        bool updateVersion(const Version& version);
        bool deleteVersion(int version_id);
        Version getVersion(int version_id);
        std::vector<Version> getVersionsByAsset(int asset_id);

        int createTask(const Task& task);
        bool updateTask(const Task& task);
        bool deleteTask(int task_id);
        Task getTask(int task_id);
        std::vector<Task> getTasksByAsset(int asset_id);
        std::vector<Task> getTasksByUser(int user_id);

        int createUser(const User& user);
        bool updateUser(const User& user);
        bool deleteUser(int user_id);
        User getUser(int user_id);
        User getUserByName(const std::string& username);
        std::vector<User> getAllUsers();

        std::vector<Asset> searchAssets(const std::string& query);
        std::vector<Task> searchTasks(const std::string& query);

        bool exportToJson(const std::string& filepath);
        bool importFromJson(const std::string& filepath);
        std::string getTableDiff(const std::string& table_name, const std::string& last_sync_id);
        bool applyDiff(const std::string& diff_json);

    private:
        sqlite3* m_db;
        std::string m_db_path;
        std::mutex m_mutex;

        int m_schema_version;

        bool checkAndUpgradeSchema();
        bool backupDatabase();

        bool executeQuery(const std::string& query);
        int getLastInsertRowId();
        sqlite3_stmt* prepareStatement(const std::string& query);
        void finalizeStatement(sqlite3_stmt* stmt);

        std::string generateSyncId();
    };

} // namespace p2p
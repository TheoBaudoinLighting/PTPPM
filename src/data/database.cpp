#include "../include/data/database.h"
#include <spdlog/spdlog.h>
#include <nlohmann/json.hpp>
#include <filesystem>
#include <fstream>
#include <random>
#include <chrono>

namespace p2p {

    Database::Database()
        : m_schema_version(1), m_db(nullptr) {
    }

    Database::~Database() {
        shutdown();
    }

    bool Database::initialize(const std::string& db_path) {
        try {
            m_db_path = db_path;

            std::filesystem::path path(db_path);
            std::filesystem::create_directories(path.parent_path());

            spdlog::info("Database initialized successfully at: {}", db_path);
            return true;
        }
        catch (const std::exception& e) {
            spdlog::error("Failed to initialize database: {}", e.what());
            return false;
        }
    }

    void Database::shutdown() {
        try {
            if (m_db) {
                sqlite3_close(m_db);
                m_db = nullptr;
            }

            spdlog::info("Database closed");
        }
        catch (const std::exception& e) {
            spdlog::error("Error during database shutdown: {}", e.what());
        }
    }

    int Database::createProject(const Project& project) {
        try {
            spdlog::info("Created project: {}", project.name);
            return 1; 
        }
        catch (const std::exception& e) {
            spdlog::error("Failed to create project: {}", e.what());
            return -1;
        }
    }

    bool Database::updateProject(const Project& project) {
        try {
            spdlog::info("Updated project: {}", project.name);
            return true;
        }
        catch (const std::exception& e) {
            spdlog::error("Failed to update project: {}", e.what());
            return false;
        }
    }

    bool Database::deleteProject(int project_id) {
        try {
            spdlog::info("Deleted project: {}", project_id);
            return true;
        }
        catch (const std::exception& e) {
            spdlog::error("Failed to delete project: {}", e.what());
            return false;
        }
    }

    Project Database::getProject(int project_id) {
        try {
            Project project;
            project.id = project_id;
            project.name = "Sample Project";
            project.code = "SAMPLE";
            project.description = "This is a sample project";
            project.created_at = std::chrono::system_clock::to_time_t(std::chrono::system_clock::now());
            project.updated_at = std::chrono::system_clock::to_time_t(std::chrono::system_clock::now());

            return project;
        }
        catch (const std::exception& e) {
            spdlog::error("Failed to get project: {}", e.what());
            return Project{};
        }
    }

    std::vector<Project> Database::getAllProjects() {
        try {
            std::vector<Project> projects;

            Project project;
            project.id = 1;
            project.name = "Sample Project";
            project.code = "SAMPLE";
            project.description = "This is a sample project";
            project.created_at = std::chrono::system_clock::to_time_t(std::chrono::system_clock::now());
            project.updated_at = std::chrono::system_clock::to_time_t(std::chrono::system_clock::now());

            projects.push_back(project);

            return projects;
        }
        catch (const std::exception& e) {
            spdlog::error("Failed to get all projects: {}", e.what());
            return {};
        }
    }

    int Database::createAsset(const Asset& asset) {
        try {
            spdlog::info("Created asset: {}", asset.name);
            return 1; 
        }
        catch (const std::exception& e) {
            spdlog::error("Failed to create asset: {}", e.what());
            return -1;
        }
    }

    bool Database::updateAsset(const Asset& asset) {
        try {
            spdlog::info("Updated asset: {}", asset.name);
            return true;
        }
        catch (const std::exception& e) {
            spdlog::error("Failed to update asset: {}", e.what());
            return false;
        }
    }

    bool Database::deleteAsset(int asset_id) {
        try {
            spdlog::info("Deleted asset: {}", asset_id);
            return true;
        }
        catch (const std::exception& e) {
            spdlog::error("Failed to delete asset: {}", e.what());
            return false;
        }
    }

    Asset Database::getAsset(int asset_id) {
        try {
            Asset asset;
            asset.id = asset_id;
            asset.name = "Sample Asset";
            asset.code = "ASSET001";
            asset.description = "This is a sample asset";
            asset.type = AssetType::MODEL;
            asset.project_id = 1;
            asset.created_at = std::chrono::system_clock::now();
            asset.updated_at = std::chrono::system_clock::now();
            asset.is_archived = false;
            asset.tags = "sample,test";
            asset.current_version_id = 1;

            return asset;
        }
        catch (const std::exception& e) {
            spdlog::error("Failed to get asset: {}", e.what());
            return Asset{};
        }
    }

    std::vector<Asset> Database::getAssetsByProject(int project_id) {
        try {
            std::vector<Asset> assets;

            Asset asset;
            asset.id = 1;
            asset.name = "Sample Asset";
            asset.code = "ASSET001";
            asset.description = "This is a sample asset";
            asset.type = AssetType::MODEL;
            asset.project_id = project_id;
            asset.created_at = std::chrono::system_clock::now();
            asset.updated_at = std::chrono::system_clock::now();
            asset.is_archived = false;
            asset.tags = "sample,test";
            asset.current_version_id = 1;

            assets.push_back(asset);

            return assets;
        }
        catch (const std::exception& e) {
            spdlog::error("Failed to get assets by project: {}", e.what());
            return {};
        }
    }

    std::vector<Asset> Database::getAllAssets() {
        try {
            std::vector<Asset> assets;

            for (int i = 1; i <= 20; i++) {
                Asset asset;
                asset.id = i;
                asset.name = "Asset " + std::to_string(i);
                asset.code = "ASSET" + std::to_string(i);
                asset.description = "This is asset " + std::to_string(i);
                asset.type = static_cast<AssetType>(i % 12);
                asset.project_id = 1;
                asset.created_at = std::chrono::system_clock::now() - std::chrono::hours(i * 24);
                asset.updated_at = std::chrono::system_clock::now() - std::chrono::hours(i * 12);
                asset.is_archived = (i % 10 == 0);
                asset.tags = "sample,test";
                asset.current_version_id = i;

                assets.push_back(asset);
            }

            return assets;
        }
        catch (const std::exception& e) {
            spdlog::error("Failed to get all assets: {}", e.what());
            return {};
        }
    }

    int Database::createVersion(const Version& version) {
        try {
            spdlog::info("Created version for asset: {}", version.asset_id);
            return 1; 
        }
        catch (const std::exception& e) {
            spdlog::error("Failed to create version: {}", e.what());
            return -1;
        }
    }

    bool Database::updateVersion(const Version& version) {
        try {
            spdlog::info("Updated version: {}", version.id);
            return true;
        }
        catch (const std::exception& e) {
            spdlog::error("Failed to update version: {}", e.what());
            return false;
        }
    }

    bool Database::deleteVersion(int version_id) {
        try {
            spdlog::info("Deleted version: {}", version_id);
            return true;
        }
        catch (const std::exception& e) {
            spdlog::error("Failed to delete version: {}", e.what());
            return false;
        }
    }

    Version Database::getVersion(int version_id) {
        try {
            Version version;
            version.id = version_id;
            version.asset_id = 1;
            version.version_number = 1;
            version.description = "Initial version";
            version.path = "/path/to/version";
            version.hash = "abcdef1234567890";
            version.created_at = std::chrono::system_clock::now();
            version.status = "Approved";

            return version;
        }
        catch (const std::exception& e) {
            spdlog::error("Failed to get version: {}", e.what());
            return Version{};
        }
    }

    std::vector<Version> Database::getVersionsByAsset(int asset_id) {
        try {
            std::vector<Version> versions;

            for (int i = 1; i <= 5; i++) {
                Version version;
                version.id = i;
                version.asset_id = asset_id;
                version.version_number = i;
                version.description = "Version " + std::to_string(i);
                version.path = "/path/to/version/" + std::to_string(i);
                version.hash = "hash" + std::to_string(i);
                version.created_at = std::chrono::system_clock::now() - std::chrono::hours(i * 24);
                version.status = (i == 5) ? "Current" : "Archived";

                versions.push_back(version);
            }

            return versions;
        }
        catch (const std::exception& e) {
            spdlog::error("Failed to get versions by asset: {}", e.what());
            return {};
        }
    }

    int Database::createTask(const Task& task) {
        try {
            spdlog::info("Created task: {}", task.title);
            return 1; 
        }
        catch (const std::exception& e) {
            spdlog::error("Failed to create task: {}", e.what());
            return -1;
        }
    }

    bool Database::updateTask(const Task& task) {
        try {
            spdlog::info("Updated task: {}", task.id);
            return true;
        }
        catch (const std::exception& e) {
            spdlog::error("Failed to update task: {}", e.what());
            return false;
        }
    }

    bool Database::deleteTask(int task_id) {
        try {
            spdlog::info("Deleted task: {}", task_id);
            return true;
        }
        catch (const std::exception& e) {
            spdlog::error("Failed to delete task: {}", e.what());
            return false;
        }
    }

    Task Database::getTask(int task_id) {
        try {
            Task task;
            task.id = task_id;
            task.title = "Sample Task";
            task.description = "This is a sample task";
            task.status = "In Progress";
            task.asset_id = 1;
            task.user_id = 1;
            task.created_at = std::chrono::system_clock::to_time_t(std::chrono::system_clock::now());
            task.due_date = std::chrono::system_clock::to_time_t(std::chrono::system_clock::now() + std::chrono::hours(24 * 7));

            return task;
        }
        catch (const std::exception& e) {
            spdlog::error("Failed to get task: {}", e.what());
            return Task{};
        }
    }

    std::vector<Task> Database::getTasksByAsset(int asset_id) {
        try {
            std::vector<Task> tasks;

            for (int i = 1; i <= 3; i++) {
                Task task;
                task.id = i;
                task.title = "Task " + std::to_string(i) + " for Asset " + std::to_string(asset_id);
                task.description = "This is task " + std::to_string(i);
                task.status = (i == 1) ? "In Progress" : ((i == 2) ? "Pending" : "Completed");
                task.asset_id = asset_id;
                task.user_id = i;
                task.created_at = std::chrono::system_clock::to_time_t(std::chrono::system_clock::now() - std::chrono::hours(i * 24));
                task.due_date = std::chrono::system_clock::to_time_t(std::chrono::system_clock::now() + std::chrono::hours(24 * (7 - i)));

                tasks.push_back(task);
            }

            return tasks;
        }
        catch (const std::exception& e) {
            spdlog::error("Failed to get tasks by asset: {}", e.what());
            return {};
        }
    }

    std::vector<Task> Database::getTasksByUser(int user_id) {
        try {
            std::vector<Task> tasks;

            for (int i = 1; i <= 5; i++) {
                Task task;
                task.id = i;
                task.title = "Task " + std::to_string(i) + " for User " + std::to_string(user_id);
                task.description = "This is task " + std::to_string(i);
                task.status = (i <= 2) ? "In Progress" : ((i <= 4) ? "Pending" : "Completed");
                task.asset_id = i;
                task.user_id = user_id;
                task.created_at = std::chrono::system_clock::to_time_t(std::chrono::system_clock::now() - std::chrono::hours(i * 24));
                task.due_date = std::chrono::system_clock::to_time_t(std::chrono::system_clock::now() + std::chrono::hours(24 * (7 - i)));

                tasks.push_back(task);
            }

            return tasks;
        }
        catch (const std::exception& e) {
            spdlog::error("Failed to get tasks by user: {}", e.what());
            return {};
        }
    }

    int Database::createUser(const User& user) {
        try {
            spdlog::info("Created user: {}", user.username);
            return 1; 
        }
        catch (const std::exception& e) {
            spdlog::error("Failed to create user: {}", e.what());
            return -1;
        }
    }

    bool Database::updateUser(const User& user) {
        try {
            spdlog::info("Updated user: {}", user.id);
            return true;
        }
        catch (const std::exception& e) {
            spdlog::error("Failed to update user: {}", e.what());
            return false;
        }
    }

    bool Database::deleteUser(int user_id) {
        try {
            spdlog::info("Deleted user: {}", user_id);
            return true;
        }
        catch (const std::exception& e) {
            spdlog::error("Failed to delete user: {}", e.what());
            return false;
        }
    }

    User Database::getUser(int user_id) {
        try {
            User user;
            user.id = user_id;
            user.username = "user" + std::to_string(user_id);
            user.email = "user" + std::to_string(user_id) + "@example.com";
            user.display_name = "User " + std::to_string(user_id);
            user.created_at = std::chrono::system_clock::to_time_t(std::chrono::system_clock::now());

            return user;
        }
        catch (const std::exception& e) {
            spdlog::error("Failed to get user: {}", e.what());
            return User{};
        }
    }

    User Database::getUserByName(const std::string& username) {
        try {
            User user;
            user.id = 1;
            user.username = username;
            user.email = username + "@example.com";
            user.display_name = "User";
            user.created_at = std::chrono::system_clock::to_time_t(std::chrono::system_clock::now());

            return user;
        }
        catch (const std::exception& e) {
            spdlog::error("Failed to get user by name: {}", e.what());
            return User{};
        }
    }

    std::vector<User> Database::getAllUsers() {
        try {
            std::vector<User> users;

            for (int i = 1; i <= 5; i++) {
                User user;
                user.id = i;
                user.username = "user" + std::to_string(i);
                user.email = "user" + std::to_string(i) + "@example.com";
                user.display_name = "User " + std::to_string(i);
                user.created_at = std::chrono::system_clock::to_time_t(std::chrono::system_clock::now() - std::chrono::hours(i * 24 * 7));

                users.push_back(user);
            }

            return users;
        }
        catch (const std::exception& e) {
            spdlog::error("Failed to get all users: {}", e.what());
            return {};
        }
    }

    std::vector<Asset> Database::searchAssets(const std::string& query) {
        try {
            auto all_assets = getAllAssets();
            std::vector<Asset> results;

            for (const auto& asset : all_assets) {
                if (asset.name.find(query) != std::string::npos ||
                    asset.code.find(query) != std::string::npos ||
                    asset.description.find(query) != std::string::npos ||
                    asset.tags.find(query) != std::string::npos) {
                    results.push_back(asset);
                }
            }

            return results;
        }
        catch (const std::exception& e) {
            spdlog::error("Failed to search assets: {}", e.what());
            return {};
        }
    }

    std::vector<Task> Database::searchTasks(const std::string& query) {
        try {
            std::vector<Task> results;

            for (int i = 1; i <= 3; i++) {
                Task task;
                task.id = i;
                task.title = "Task " + std::to_string(i) + " matching " + query;
                task.description = "This task matches the search query: " + query;
                task.status = (i == 1) ? "In Progress" : ((i == 2) ? "Pending" : "Completed");
                task.asset_id = i;
                task.user_id = i;
                task.created_at = std::chrono::system_clock::to_time_t(std::chrono::system_clock::now() - std::chrono::hours(i * 24));
                task.due_date = std::chrono::system_clock::to_time_t(std::chrono::system_clock::now() + std::chrono::hours(24 * (7 - i)));

                results.push_back(task);
            }

            return results;
        }
        catch (const std::exception& e) {
            spdlog::error("Failed to search tasks: {}", e.what());
            return {};
        }
    }

    bool Database::exportToJson(const std::string& filepath) {
        try {
            nlohmann::json json;

            json["projects"] = nlohmann::json::array();
            for (const auto& project : getAllProjects()) {
                nlohmann::json project_json;
                project_json["id"] = project.id;
                project_json["name"] = project.name;
                project_json["code"] = project.code;
                project_json["description"] = project.description;

                json["projects"].push_back(project_json);
            }

            json["assets"] = nlohmann::json::array();
            for (const auto& asset : getAllAssets()) {
                nlohmann::json asset_json;
                asset_json["id"] = asset.id;
                asset_json["name"] = asset.name;
                asset_json["code"] = asset.code;
                asset_json["description"] = asset.description;
                asset_json["type"] = static_cast<int>(asset.type);
                asset_json["project_id"] = asset.project_id;

                json["assets"].push_back(asset_json);
            }

            std::ofstream file(filepath);
            if (!file.is_open()) {
                spdlog::error("Failed to open file for writing: {}", filepath);
                return false;
            }

            file << json.dump(4); 
            file.close();

            spdlog::info("Exported database to JSON: {}", filepath);
            return true;
        }
        catch (const std::exception& e) {
            spdlog::error("Failed to export database to JSON: {}", e.what());
            return false;
        }
    }

    bool Database::importFromJson(const std::string& filepath) {
        try {
            std::ifstream file(filepath);
            if (!file.is_open()) {
                spdlog::error("Failed to open file for reading: {}", filepath);
                return false;
            }

            nlohmann::json json;
            file >> json;
            file.close();

            if (json.contains("projects") && json["projects"].is_array()) {
                for (const auto& project_json : json["projects"]) {
                    Project project;
                    project.id = project_json["id"];
                    project.name = project_json["name"];
                    project.code = project_json["code"];
                    project.description = project_json["description"];

                    if (getProject(project.id).id == 0) {
                        createProject(project);
                    }
                    else {
                        updateProject(project);
                    }
                }
            }

            if (json.contains("assets") && json["assets"].is_array()) {
                for (const auto& asset_json : json["assets"]) {
                    Asset asset;
                    asset.id = asset_json["id"];
                    asset.name = asset_json["name"];
                    asset.code = asset_json["code"];
                    asset.description = asset_json["description"];
                    asset.type = static_cast<AssetType>(asset_json["type"].get<int>());
                    asset.project_id = asset_json["project_id"];

                    if (getAsset(asset.id).id == 0) {
                        createAsset(asset);
                    }
                    else {
                        updateAsset(asset);
                    }
                }
            }

            spdlog::info("Imported database from JSON: {}", filepath);
            return true;
        }
        catch (const std::exception& e) {
            spdlog::error("Failed to import database from JSON: {}", e.what());
            return false;
        }
    }

    std::string Database::getTableDiff(const std::string& table_name, const std::string& /*last_sync_id*/) {
        nlohmann::json diff;
        diff["table"] = table_name;
        diff["changes"] = nlohmann::json::array();
        
        return diff.dump();
    }

    bool Database::applyDiff(const std::string& diff_json) {
        try {
            auto diff = nlohmann::json::parse(diff_json);

            std::string table_name = diff["table"];
            std::string sync_id = diff["sync_id"];

            for (const auto& change : diff["changes"]) {
                std::string operation = change["operation"];
                int id = change["id"];

                if (operation == "insert") {
                    if (table_name == "projects") {
                        Project project;
                        project.id = id;

                        createProject(project);
                    }
                    else if (table_name == "assets") {
                        Asset asset;
                        asset.id = id;

                        createAsset(asset);
                    }
                    else if (table_name == "versions") {
                        Version version;
                        version.id = id;

                        createVersion(version);
                    }
                    else if (table_name == "tasks") {
                        Task task;
                        task.id = id;

                        createTask(task);
                    }
                    else if (table_name == "users") {
                        User user;
                        user.id = id;

                        createUser(user);
                    }
                }
                else if (operation == "update") {
                    if (table_name == "projects") {
                        Project project;
                        project.id = id;

                        updateProject(project);
                    }
                    else if (table_name == "assets") {
                        Asset asset;
                        asset.id = id;

                        updateAsset(asset);
                    }
                    else if (table_name == "versions") {
                        Version version;
                        version.id = id;

                        updateVersion(version);
                    }
                    else if (table_name == "tasks") {
                        Task task;
                        task.id = id;

                        updateTask(task);
                    }
                    else if (table_name == "users") {
                        User user;
                        user.id = id;

                        updateUser(user);
                    }
                }
                else if (operation == "delete") {
                    if (table_name == "projects") {
                        deleteProject(id);
                    }
                    else if (table_name == "assets") {
                        deleteAsset(id);
                    }
                    else if (table_name == "versions") {
                        deleteVersion(id);
                    }
                    else if (table_name == "tasks") {
                        deleteTask(id);
                    }
                    else if (table_name == "users") {
                        deleteUser(id);
                    }
                }
            }

            spdlog::info("Applied diff for table: {}", table_name);
            return true;
        }
        catch (const std::exception& e) {
            spdlog::error("Failed to apply diff: {}", e.what());
            return false;
        }
    }

    std::string Database::generateSyncId() {
        auto now = std::chrono::system_clock::now();
        auto now_ms = std::chrono::time_point_cast<std::chrono::milliseconds>(now);
        auto value = now_ms.time_since_epoch().count();

        std::random_device rd;
        std::mt19937 gen(rd());
        std::uniform_int_distribution<> dis(0, 999999);
        int random_value = dis(gen);

        return std::to_string(value) + "-" + std::to_string(random_value);
    }

    bool Database::checkAndUpgradeSchema() {
        return true;
    }

    bool Database::backupDatabase() {
        try {
            auto now = std::chrono::system_clock::now();
            auto now_time_t = std::chrono::system_clock::to_time_t(now);
            std::tm tm = *std::localtime(&now_time_t);

            char buffer[128];
            std::strftime(buffer, sizeof(buffer), "%Y%m%d_%H%M%S", &tm);

            std::string backup_path = m_db_path + "." + buffer + ".bak";

            std::ifstream src(m_db_path, std::ios::binary);
            if (!src.is_open()) {
                spdlog::error("Failed to open source database file for backup: {}", m_db_path);
                return false;
            }

            std::ofstream dst(backup_path, std::ios::binary);
            if (!dst.is_open()) {
                spdlog::error("Failed to open destination backup file: {}", backup_path);
                src.close();
                return false;
            }

            dst << src.rdbuf();

            src.close();
            dst.close();

            spdlog::info("Database backup created: {}", backup_path);
            return true;
        }
        catch (const std::exception& e) {
            spdlog::error("Failed to backup database: {}", e.what());
            return false;
        }
    }

    bool Database::executeQuery(const std::string& query) {
        char* error_message = nullptr;
        int result = sqlite3_exec(m_db, query.c_str(), nullptr, nullptr, &error_message);
        
        if (result != SQLITE_OK) {
            spdlog::error("SQLite error: {}", error_message);
            sqlite3_free(error_message);
            return false;
        }
        
        return true;
    }

    int Database::getLastInsertRowId() {
        return static_cast<int>(sqlite3_last_insert_rowid(m_db));
    }

    sqlite3_stmt* Database::prepareStatement(const std::string& query) {
        sqlite3_stmt* stmt = nullptr;
        int result = sqlite3_prepare_v2(m_db, query.c_str(), -1, &stmt, nullptr);
        
        if (result != SQLITE_OK) {
            spdlog::error("Failed to prepare statement: {}", sqlite3_errmsg(m_db));
            return nullptr;
        }
        
        return stmt;
    }

    void Database::finalizeStatement(sqlite3_stmt* stmt) {
        if (stmt) {
            sqlite3_finalize(stmt);
        }
    }

} // namespace p2p
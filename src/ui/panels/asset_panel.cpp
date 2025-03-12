#include "../include/ui/panels/asset_panel.h"
#include <imgui.h>
#include <spdlog/spdlog.h>

namespace p2p {

    AssetPanel::AssetPanel(Database* database, VersionManager* version_manager)
        : Panel("Assets"),
        m_database(database),
        m_version_manager(version_manager),
        m_filter(""),
        m_sort_column(0),
        m_sort_ascending(true),
        m_selected_asset_id(-1),
        m_list_view(true) {
    }

    void AssetPanel::render() {
        ImGui::SetNextWindowSize(ImVec2(800, 600), ImGuiCond_FirstUseEver);
        if (ImGui::Begin(m_title.c_str(), &m_is_visible)) {
            renderToolbar();

            ImGui::Separator();

            if (m_list_view) {
                renderAssetList();
            }
            else {
                renderAssetGrid();
            }

            if (m_selected_asset_id >= 0) {
                ImGui::SameLine();
                renderAssetDetails();
            }
        }
        ImGui::End();
    }

    void AssetPanel::renderToolbar() {
        if (ImGui::Button(m_list_view ? "Grid View" : "List View")) {
            m_list_view = !m_list_view;
        }

        ImGui::SameLine();

        if (ImGui::Button("New Asset")) {
            createNewAsset();
        }

        ImGui::SameLine();

        ImGui::SetNextItemWidth(200);
        char filter_buffer[256];
        strncpy(filter_buffer, m_filter.c_str(), sizeof(filter_buffer) - 1);
        filter_buffer[sizeof(filter_buffer) - 1] = '\0';
        if (ImGui::InputTextWithHint("##filter", "Filter assets...", filter_buffer, sizeof(filter_buffer))) {
            m_filter = filter_buffer;
        }

        ImGui::SameLine();

        if (ImGui::Button("Refresh")) {
        }
    }

    void AssetPanel::renderAssetList() {
        ImGui::BeginChild("AssetListPane", ImVec2(m_selected_asset_id >= 0 ? ImGui::GetContentRegionAvail().x * 0.5f : ImGui::GetContentRegionAvail().x, ImGui::GetContentRegionAvail().y), true);

        ImGui::Columns(5, "AssetTable", true);
        if (ImGui::Selectable("Name", m_sort_column == 0)) {
            if (m_sort_column == 0) {
                m_sort_ascending = !m_sort_ascending;
            }
            else {
                m_sort_column = 0;
                m_sort_ascending = true;
            }
        }
        ImGui::NextColumn();

        if (ImGui::Selectable("Type", m_sort_column == 1)) {
            if (m_sort_column == 1) {
                m_sort_ascending = !m_sort_ascending;
            }
            else {
                m_sort_column = 1;
                m_sort_ascending = true;
            }
        }
        ImGui::NextColumn();

        if (ImGui::Selectable("Project", m_sort_column == 2)) {
            if (m_sort_column == 2) {
                m_sort_ascending = !m_sort_ascending;
            }
            else {
                m_sort_column = 2;
                m_sort_ascending = true;
            }
        }
        ImGui::NextColumn();

        if (ImGui::Selectable("Version", m_sort_column == 3)) {
            if (m_sort_column == 3) {
                m_sort_ascending = !m_sort_ascending;
            }
            else {
                m_sort_column = 3;
                m_sort_ascending = true;
            }
        }
        ImGui::NextColumn();

        if (ImGui::Selectable("Updated", m_sort_column == 4)) {
            if (m_sort_column == 4) {
                m_sort_ascending = !m_sort_ascending;
            }
            else {
                m_sort_column = 4;
                m_sort_ascending = true;
            }
        }
        ImGui::NextColumn();

        ImGui::Separator();

        auto assets = m_database->getAllAssets();

        if (!m_filter.empty()) {
            assets.erase(
                std::remove_if(assets.begin(), assets.end(),
                    [this](const Asset& asset) {
                        return asset.name.find(m_filter) == std::string::npos &&
                            asset.code.find(m_filter) == std::string::npos &&
                            asset.tags.find(m_filter) == std::string::npos;
                    }
                ),
                assets.end()
            );
        }

        std::sort(assets.begin(), assets.end(),
            [this](const Asset& a, const Asset& b) {
                if (m_sort_column == 0) {
                    return m_sort_ascending ? a.name < b.name : a.name > b.name;
                }
                else if (m_sort_column == 1) {
                    return m_sort_ascending ? a.type < b.type : a.type > b.type;
                }
                else if (m_sort_column == 2) {
                    return m_sort_ascending ? a.project_id < b.project_id : a.project_id > b.project_id;
                }
                else if (m_sort_column == 3) {
                    return m_sort_ascending ? a.current_version_id < b.current_version_id : a.current_version_id > b.current_version_id;
                }
                else if (m_sort_column == 4) {
                    return m_sort_ascending ? a.updated_at < b.updated_at : a.updated_at > b.updated_at;
                }
                return false;
            }
        );

        for (const auto& asset : assets) {
            bool is_selected = m_selected_asset_id == asset.id;
            if (ImGui::Selectable(asset.name.c_str(), is_selected, ImGuiSelectableFlags_SpanAllColumns)) {
                m_selected_asset_id = asset.id;
            }

            if (ImGui::IsItemHovered() && ImGui::IsMouseReleased(1)) {
                m_selected_asset_id = asset.id;
                ImGui::OpenPopup("AssetContextMenu");
            }

            ImGui::NextColumn();

            ImGui::Text("%s", assetTypeToString(asset.type).c_str());
            ImGui::NextColumn();

            auto project = m_database->getProject(asset.project_id);
            ImGui::Text("%s", project.name.c_str());
            ImGui::NextColumn();

            ImGui::Text("v%d", asset.current_version_id);
            ImGui::NextColumn();

            auto updated_time = std::chrono::system_clock::to_time_t(asset.updated_at);
            char time_buf[64];
            std::strftime(time_buf, sizeof(time_buf), "%Y-%m-%d %H:%M", std::localtime(&updated_time));
            ImGui::Text("%s", time_buf);
            ImGui::NextColumn();
        }

        ImGui::Columns(1);

        renderContextMenu();

        ImGui::EndChild();
    }

    void AssetPanel::renderAssetGrid() {
        ImGui::BeginChild("AssetGridPane", ImVec2(m_selected_asset_id >= 0 ? ImGui::GetContentRegionAvail().x * 0.5f : ImGui::GetContentRegionAvail().x, ImGui::GetContentRegionAvail().y), true);

        auto assets = m_database->getAllAssets();

        if (!m_filter.empty()) {
            assets.erase(
                std::remove_if(assets.begin(), assets.end(),
                    [this](const Asset& asset) {
                        return asset.name.find(m_filter) == std::string::npos &&
                            asset.code.find(m_filter) == std::string::npos &&
                            asset.tags.find(m_filter) == std::string::npos;
                    }
                ),
                assets.end()
            );
        }

        std::sort(assets.begin(), assets.end(),
            [this](const Asset& a, const Asset& b) {
                if (m_sort_column == 0) {
                    return m_sort_ascending ? a.name < b.name : a.name > b.name;
                }
                else if (m_sort_column == 1) {
                    return m_sort_ascending ? a.type < b.type : a.type > b.type;
                }
                else if (m_sort_column == 2) {
                    return m_sort_ascending ? a.project_id < b.project_id : a.project_id > b.project_id;
                }
                else if (m_sort_column == 3) {
                    return m_sort_ascending ? a.current_version_id < b.current_version_id : a.current_version_id > b.current_version_id;
                }
                else if (m_sort_column == 4) {
                    return m_sort_ascending ? a.updated_at < b.updated_at : a.updated_at > b.updated_at;
                }
                return false;
            }
        );

        float cell_size = 120.0f;
        float cell_padding = 8.0f;
        float panel_width = ImGui::GetContentRegionAvail().x;
        int cells_per_row = static_cast<int>((panel_width - cell_padding) / (cell_size + cell_padding));
        cells_per_row = std::max(cells_per_row, 1);

        int cell_index = 0;
        for (const auto& asset : assets) {
            ImGui::PushID(asset.id);

            int row = cell_index / cells_per_row;
            int col = cell_index % cells_per_row;
            float x = col * (cell_size + cell_padding) + cell_padding;
            float y = row * (cell_size + cell_padding) + cell_padding;

            ImGui::SetCursorPos(ImVec2(x, y));

            bool is_selected = m_selected_asset_id == asset.id;
            if (is_selected) {
                ImGui::PushStyleColor(ImGuiCol_Button, ImGui::GetStyle().Colors[ImGuiCol_ButtonActive]);
            }

            if (ImGui::Button("", ImVec2(cell_size, cell_size))) {
                m_selected_asset_id = asset.id;
            }

            if (is_selected) {
                ImGui::PopStyleColor();
            }

            if (ImGui::IsItemHovered() && ImGui::IsMouseReleased(1)) {
                m_selected_asset_id = asset.id;
                ImGui::OpenPopup("AssetContextMenu");
            }

            ImGui::SetCursorPos(ImVec2(x + 10, y + 10));
            ImGui::Text("%s", assetTypeToString(asset.type).c_str());

            ImGui::SetCursorPos(ImVec2(x + 5, y + cell_size - 20));
            ImGui::TextWrapped("%s", asset.name.c_str());

            ImGui::PopID();

            cell_index++;
        }

        renderContextMenu();

        ImGui::EndChild();
    }

    void AssetPanel::renderAssetDetails() {
        if (m_selected_asset_id < 0) return;

        Asset asset = m_database->getAsset(m_selected_asset_id);

        ImGui::BeginChild("AssetDetailsPane", ImVec2(0, 0), true);

        ImGui::Text("Asset Details");
        ImGui::Separator();

        ImGui::Text("Name: %s", asset.name.c_str());
        ImGui::Text("Code: %s", asset.code.c_str());
        ImGui::Text("Type: %s", assetTypeToString(asset.type).c_str());

        auto project = m_database->getProject(asset.project_id);
        ImGui::Text("Project: %s", project.name.c_str());

        auto created_time = std::chrono::system_clock::to_time_t(asset.created_at);
        auto updated_time = std::chrono::system_clock::to_time_t(asset.updated_at);
        char created_buf[64];
        char updated_buf[64];
        std::strftime(created_buf, sizeof(created_buf), "%Y-%m-%d %H:%M", std::localtime(&created_time));
        std::strftime(updated_buf, sizeof(updated_buf), "%Y-%m-%d %H:%M", std::localtime(&updated_time));

        ImGui::Text("Created: %s", created_buf);
        ImGui::Text("Updated: %s", updated_buf);

        ImGui::Separator();

        ImGui::Text("Description:");
        ImGui::BeginChild("Description", ImVec2(0, 100), true);
        ImGui::TextWrapped("%s", asset.description.c_str());
        ImGui::EndChild();

        ImGui::Text("Tags: %s", asset.tags.c_str());

        ImGui::Separator();

        ImGui::Text("Versions:");
        auto versions = m_database->getVersionsByAsset(asset.id);

        ImGui::BeginChild("Versions", ImVec2(0, 200), true);
        if (ImGui::BeginTable("VersionsTable", 3, ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg)) {
            ImGui::TableSetupColumn("Version");
            ImGui::TableSetupColumn("Date");
            ImGui::TableSetupColumn("Status");
            ImGui::TableHeadersRow();

            for (const auto& version : versions) {
                ImGui::TableNextRow();

                ImGui::TableNextColumn();
                ImGui::Text("v%d", version.version_number);

                ImGui::TableNextColumn();
                auto version_time = std::chrono::system_clock::to_time_t(version.created_at);
                char time_buf[64];
                std::strftime(time_buf, sizeof(time_buf), "%Y-%m-%d %H:%M", std::localtime(&version_time));
                ImGui::Text("%s", time_buf);

                ImGui::TableNextColumn();
                ImGui::Text("%s", version.status.c_str());
            }

            ImGui::EndTable();
        }
        ImGui::EndChild();

        if (ImGui::Button("New Version")) {
        }

        ImGui::SameLine();

        if (ImGui::Button("Edit")) {
        }

        ImGui::SameLine();

        if (ImGui::Button("Delete")) {
            deleteSelectedAsset();
        }

        ImGui::EndChild();
    }

    void AssetPanel::renderContextMenu() {
        if (ImGui::BeginPopup("AssetContextMenu")) {
            if (ImGui::MenuItem("Open")) {
            }

            if (ImGui::MenuItem("Edit")) {
                renameSelectedAsset();
            }

            if (ImGui::MenuItem("Delete")) {
                deleteSelectedAsset();
            }

            ImGui::Separator();

            if (ImGui::MenuItem("New Version")) {
            }

            if (ImGui::MenuItem("Version History")) {
                showVersionHistory();
            }

            ImGui::Separator();

            if (ImGui::MenuItem("Copy")) {
                copySelectedAsset();
            }

            ImGui::EndPopup();
        }
    }

    void AssetPanel::createNewAsset() {
    }

    void AssetPanel::deleteSelectedAsset() {
        if (m_selected_asset_id < 0) return;

        ImGui::OpenPopup("Confirm Delete");
    }

    void AssetPanel::renameSelectedAsset() {
        if (m_selected_asset_id < 0) return;

        ImGui::OpenPopup("Rename Asset");
    }

    void AssetPanel::copySelectedAsset() {
        if (m_selected_asset_id < 0) return;

        Asset asset = m_database->getAsset(m_selected_asset_id);
        asset.name += " (Copy)";
        asset.id = 0;
        asset.created_at = std::chrono::system_clock::now();
        asset.updated_at = std::chrono::system_clock::now();

        m_database->createAsset(asset);
    }

    void AssetPanel::showVersionHistory() {
        if (m_selected_asset_id < 0) return;

        ImGui::OpenPopup("Version History");
    }

} // namespace p2p
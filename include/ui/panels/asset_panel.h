#pragma once

#include "panel.h"
#include "data/database.h"
#include "version_control/version_manager.h"

namespace p2p {

    class AssetPanel : public Panel {
    public:
        AssetPanel(Database* database, VersionManager* version_manager);
        ~AssetPanel() override = default;

        void render() override;

    private:
        Database* m_database;
        VersionManager* m_version_manager;

        std::string m_filter;
        int m_sort_column;
        bool m_sort_ascending;

        int m_selected_asset_id;

        bool m_list_view;

        void renderToolbar();
        void renderAssetList();
        void renderAssetGrid();
        void renderAssetDetails();
        void renderContextMenu();

        void createNewAsset();
        void deleteSelectedAsset();
        void renameSelectedAsset();
        void copySelectedAsset();
        void showVersionHistory();
    };

} // namespace p2p
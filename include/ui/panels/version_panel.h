#pragma once

#include "ui/panels/panel.h"
#include "version_control/version_manager.h"

namespace p2p {

    class VersionPanel : public Panel {
    public:
        VersionPanel(VersionManager* version_manager);
        ~VersionPanel() override = default;

        void render() override;

    private:
        VersionManager* m_version_manager;
        
        bool m_show_commit_dialog = false;
        std::string m_commit_message;
        std::string m_selected_branch;
        std::string m_new_branch_name;
        
        void renderBranchList();
        void renderCommitHistory();
        void renderCommitDialog();
        void renderToolbar();
        
        void createBranch();
        void switchBranch();
        void commit();
        void push();
        void pull();
        void merge();
    };

} // namespace p2p 
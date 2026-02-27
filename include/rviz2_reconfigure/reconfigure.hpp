#ifndef RVIZ2_RECONFIGURE__RECONFIGURE_HPP_
#define RVIZ2_RECONFIGURE__RECONFIGURE_HPP_

#ifndef Q_MOC_RUN

#include <fstream>

#include <rclcpp/rclcpp.hpp>
#include <rclcpp/logger.hpp>
#include <rviz_common/panel.hpp>
#include <rviz_common/config.hpp>
#include <rviz_common/display_context.hpp>
#include <yaml-cpp/yaml.h>

#include <QCheckBox>
#include <QFileDialog>
#include <QPushButton>
#include <QStyledItemDelegate>
#include <QTreeWidget>

#include "rviz2_reconfigure/param_dialog.hpp"
#include "rviz2_reconfigure/delegate.hpp"

#include "ui_reconfigure.h"

#endif // Q_MOC_RUN

namespace Ui
{
    class Reconfigure;
} // namespace Ui

namespace rviz2_reconfigure
{
    class RViz2Reconfigure : public rviz_common::Panel
    {
        Q_OBJECT
    public:
        explicit RViz2Reconfigure(QWidget *parent = nullptr);
        ~RViz2Reconfigure();
        void onInitialize() override;
        void load(const rviz_common::Config &config) override;
        void save(rviz_common::Config config) const override;

        enum UserRole {
            FullPathRole = Qt::UserRole,
            ParamTypeRole = Qt::UserRole + 1
        };

    protected Q_SLOTS:
        void addPushBtn__clicked();
        void refreshAllValues();
        void onItemChanged(QTreeWidgetItem *item, int column);
        void autoRefreshChkBox__CheckStateChanged(int state);
        void removePushBtn__clicked();
        void importPushBtn__clicked();
        void exportPushBtn__clicked();
        
    protected:
        QTreeWidgetItem* getOrCreateChild(QTreeWidgetItem *parent, const QString &name);
        void collectLeafItems(QTreeWidgetItem *parent, QList<QTreeWidgetItem*> &leaf_items) const;
        void loadParamsToTree(const QList<QPair<QString, QString>> &params_to_load);
        void checkAndRemoveEmptyParents(QTreeWidgetItem *parent);
        void setParamValues(const std::map<std::string, std::vector<std::pair<std::string, QTreeWidgetItem*>>> &batch_map);
        void parseYamlRecursive(
            const std::string &node_name,
            const YAML::Node &current_node,
            const std::string &prefix,
            std::map<std::string, std::map<std::string, QTreeWidgetItem*>> &registered,
            std::map<std::string, std::vector<std::pair<std::string, QTreeWidgetItem*>>> &batch_map
        );
        void serializeItem(QTreeWidgetItem *item, YAML::Emitter &out);

    private:
        rclcpp::Node::SharedPtr nh_;
        rclcpp::TimerBase::SharedPtr auto_refresh_timer_;
        Ui::Reconfigure *ui_;
        std::shared_ptr<rclcpp::Logger> logger_;
        std::vector<rclcpp::Client<rcl_interfaces::srv::GetParameters>::SharedPtr> get_params_clients_;
        std::vector<rclcpp::Client<rcl_interfaces::srv::SetParameters>::SharedPtr> set_params_clients_;
    };
    
} // namespace rviz2_reconfigure

#endif // RVIZ2_RECONFIGURE__RECONFIGURE_HPP_

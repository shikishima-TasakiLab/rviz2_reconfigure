#ifndef RVIZ2_RECONFIGURE__RECONFIGURE_HPP_
#define RVIZ2_RECONFIGURE__RECONFIGURE_HPP_

#ifndef Q_MOC_RUN
#include <rclcpp/rclcpp.hpp>
#include <rclcpp/logger.hpp>
#include <rviz_common/panel.hpp>
#include <rviz_common/config.hpp>
#include <rviz_common/display_context.hpp>

#include <QCheckBox>
#include <QDialog>
#include <QLineEdit>
#include <QPushButton>
#include <QStyledItemDelegate>
#include <QTreeWidget>

#include "ui_reconfigure.h"
#include "ui_param_dialog.h"
#endif

namespace Ui
{
    class Reconfigure;
    class ParamDialog;
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

    private:
        rclcpp::Node::SharedPtr nh_;
        rclcpp::TimerBase::SharedPtr auto_refresh_timer_;
        Ui::Reconfigure *ui_;
        std::shared_ptr<rclcpp::Logger> logger_;
        std::vector<rclcpp::Client<rcl_interfaces::srv::GetParameters>::SharedPtr> get_params_clients_;
        std::vector<rclcpp::Client<rcl_interfaces::srv::SetParameters>::SharedPtr> set_params_clients_;
    };
    

    class ParamDialog : public QDialog
    {
        Q_OBJECT
    public:
        explicit ParamDialog(rclcpp::Node::SharedPtr node_handle, const std::shared_ptr<rclcpp::Logger> &logger, rviz_common::Panel *parent = nullptr);
        ~ParamDialog();
        QList<QPair<QString, QString> > getCheckedParams() const;
    
    protected Q_SLOTS:
        void refresh();
        void onItemChanged(QTreeWidgetItem *item, int column);
    protected:
        void updateChildCheckState(QTreeWidgetItem *item, Qt::CheckState state);
        void updateParentCheckState(QTreeWidgetItem *item);
        QTreeWidgetItem* getOrCreateChild(QTreeWidgetItem *parent, const QString &name);

    private:
        rclcpp::Node::SharedPtr nh_;
        Ui::ParamDialog *ui_;
        std::shared_ptr<rclcpp::Logger> logger_;
        std::vector<rclcpp::Client<rcl_interfaces::srv::ListParameters>::SharedPtr> list_params_clients_;
    };


    class ParamEditorDelegate : public QStyledItemDelegate
    {
        Q_OBJECT
    public:
        ParamEditorDelegate(QObject *parent = nullptr) : QStyledItemDelegate(parent) {}
        QWidget* createEditor(QWidget *parent, const QStyleOptionViewItem &option, const QModelIndex &index) const override;
    };

} // namespace rviz2_reconfigure

#endif // RVIZ2_RECONFIGURE__RECONFIGURE_HPP_

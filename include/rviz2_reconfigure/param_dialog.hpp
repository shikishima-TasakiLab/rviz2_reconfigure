#ifndef RVIZ2_RECONFIGURE__PARAM_DIALOG_HPP_
#define RVIZ2_RECONFIGURE__PARAM_DIALOG_HPP_

#ifndef Q_MOC_RUN

#include <rclcpp/rclcpp.hpp>
#include <rviz_common/panel.hpp>

#include <QDialog>
#include <QPushButton>
#include <QTreeWidget>

#include "ui_param_dialog.h"

#endif // Q_MOC_RUN

namespace Ui
{
    class ParamDialog;
} // namespace Ui

namespace rviz2_reconfigure
{

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

} // namespace rviz2_reconfigure

#endif // RVIZ2_RECONFIGURE__PARAM_DIALOG_HPP_

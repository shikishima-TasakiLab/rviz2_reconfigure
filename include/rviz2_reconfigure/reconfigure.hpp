#ifndef RVIZ2_RECONFIGURE__RECONFIGURE_HPP_
#define RVIZ2_RECONFIGURE__RECONFIGURE_HPP_

#ifndef Q_MOC_RUN
#include <rclcpp/rclcpp.hpp>
#include <rviz_common/panel.hpp>
#include <rviz_common/config.hpp>
#include <rviz_common/display_context.hpp>

#include <QTimer>

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
    
    protected Q_SLOTS:
        void addPushBtn__clicked();
        QTreeWidgetItem* getOrCreateChild(QTreeWidgetItem *parent, const QString &name);

    private:
        rclcpp::Node::SharedPtr nh_;
        Ui::Reconfigure *ui_;
        QTimer *ros_timer_;
    };
    

    class ParamDialog : public QDialog
    {
        Q_OBJECT
    public:
        explicit ParamDialog(rclcpp::Node::SharedPtr node_handle, rviz_common::Panel *parent = nullptr);
        ~ParamDialog();
        QList<QPair<QString, QString> > getCheckedParams() const;
    
    protected Q_SLOTS:
        void accept() override;
        void refresh();
        QTreeWidgetItem* getOrCreateChild(QTreeWidgetItem *parent, const QString &name);
        void onItemChanged(QTreeWidgetItem *item, int column);
        void updateChildCheckState(QTreeWidgetItem *item, Qt::CheckState state);
        void updateParentCheckState(QTreeWidgetItem *item);

    private:
        rclcpp::Node::SharedPtr nh_;
        Ui::ParamDialog *ui_;
        std::vector<rclcpp::Client<rcl_interfaces::srv::ListParameters>::SharedPtr> list_params_clients_;
    };

} // namespace rviz2_reconfigure

#endif // RVIZ2_RECONFIGURE__RECONFIGURE_HPP_

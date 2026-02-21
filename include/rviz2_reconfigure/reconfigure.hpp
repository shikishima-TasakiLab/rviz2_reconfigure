#ifndef RVIZ2_RECONFIGURE__RECONFIGURE_HPP_
#define RVIZ2_RECONFIGURE__RECONFIGURE_HPP_

#ifndef Q_MOC_RUN
#include <rclcpp/rclcpp.hpp>
#include <rviz_common/panel.hpp>
#include <rviz_common/config.hpp>
#include <rviz_common/display_context.hpp>

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
    
    private:
        rclcpp::Node::SharedPtr nh_;
        Ui::Reconfigure *ui_;
    };

    class ParamDialog : public QDialog
    {
        Q_OBJECT
    public:
        explicit ParamDialog(QWidget *parent = nullptr);
        ~ParamDialog();
        QMap<QString, QList<QTreeWidgetItem*> > getCheckedParams() const;
    
    protected Q_SLOTS:
        void accept() override;
        void refreshPushBtn__clicked();

    private:
        Ui::ParamDialog *ui_;
    };

} // namespace rviz2_reconfigure

#endif // RVIZ2_RECONFIGURE__RECONFIGURE_HPP_

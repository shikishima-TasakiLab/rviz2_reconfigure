#include "rviz2_reconfigure/reconfigure.hpp"

namespace rviz2_reconfigure
{
    RViz2Reconfigure::RViz2Reconfigure(QWidget *parent)
        : rviz_common::Panel(parent), ui_(new Ui::Reconfigure())
    {
        ui_->setupUi(this);

        connect(ui_->addPushBtn, &QPushButton::clicked, this, &RViz2Reconfigure::addPushBtn__clicked);
    }

    RViz2Reconfigure::~RViz2Reconfigure()
    {
    }

    void RViz2Reconfigure::onInitialize()
    {
        nh_ = getDisplayContext()->getRosNodeAbstraction().lock()->get_raw_node();
    }

    void RViz2Reconfigure::load(const rviz_common::Config &config)
    {
        rviz_common::Panel::load(config);
    }

    void RViz2Reconfigure::save(rviz_common::Config config) const
    {
        rviz_common::Panel::save(config);
    }

    void RViz2Reconfigure::addPushBtn__clicked()
    {
        ParamDialog dialog(this);
        if(dialog.exec() == QDialog::Accepted) {
            QMap<QString, QList<QTreeWidgetItem *> > checked_params = dialog.getCheckedParams();
        }
    }


    ParamDialog::ParamDialog(QWidget *parent)
        : QDialog(parent), ui_(new Ui::ParamDialog())
    {
        ui_->setupUi(this);

        connect(ui_->refreshPushBtn, &QPushButton::clicked, this, &ParamDialog::refreshPushBtn__clicked);
    }

    ParamDialog::~ParamDialog()
    {
    }

    QMap<QString, QList<QTreeWidgetItem*> > ParamDialog::getCheckedParams() const
    {
        QMap<QString, QList<QTreeWidgetItem*> > checked_params;

        for (int i = 0; i < ui_->listNodeParam->topLevelItemCount(); ++i) {
            QTreeWidgetItem *parent_item = ui_->listNodeParam->topLevelItem(i);
            QList<QTreeWidgetItem*> checked_children;

            for (int j = 0; j < parent_item->childCount(); ++j) {
                QTreeWidgetItem *child_item = parent_item->child(j);
                if (child_item->checkState(0) == Qt::Checked) {
                    checked_children.append(child_item->clone());
                }
            }

            if (!checked_children.isEmpty()) {
                checked_params.insert(parent_item->text(0), checked_children);
            }
        }
        return checked_params;
    }

    void ParamDialog::accept()
    {
        QDialog::accept();
    }

    void ParamDialog::refreshPushBtn__clicked()
    {
    }

} // namespace rviz2_reconfigure

#include <pluginlib/class_list_macros.hpp>
PLUGINLIB_EXPORT_CLASS(rviz2_reconfigure::RViz2Reconfigure, rviz_common::Panel)

#include "rviz2_reconfigure/param_dialog.hpp"

namespace rviz2_reconfigure
{

    ParamDialog::ParamDialog(rclcpp::Node::SharedPtr node_handle, const std::shared_ptr<rclcpp::Logger> &logger, rviz_common::Panel *parent)
        : QDialog(parent), nh_(node_handle), ui_(new Ui::ParamDialog()), logger_(logger)
    {
        ui_->setupUi(this);
        ui_->listNodeParam->sortByColumn(0, Qt::AscendingOrder);

        connect(ui_->refreshPushBtn, &QPushButton::clicked, this, &ParamDialog::refresh);
        connect(ui_->listNodeParam, &QTreeWidget::itemChanged, this, &ParamDialog::onItemChanged);

        refresh();
    }

    ParamDialog::~ParamDialog()
    {
    }

    QList<QPair<QString, QString>> ParamDialog::getCheckedParams() const
    {
        QList<QPair<QString, QString>> selected;
        QTreeWidgetItemIterator it(ui_->listNodeParam);

        while (*it)
        {
            if ((*it)->checkState(0) == Qt::Checked && (*it)->childCount() == 0)
            {
                QStringList path_parts;
                QTreeWidgetItem *current = *it;

                while (current->parent() != nullptr)
                {
                    path_parts.prepend(current->text(0));
                    current = current->parent();
                }

                selected.append({current->text(0), path_parts.join(".")});
            }
            ++it;
        }
        return selected;
    }

    void ParamDialog::refresh()
    {
        ui_->listNodeParam->clear();
        list_params_clients_.clear();

        std::vector<std::string> ns_node_names = nh_->get_node_names();
        if (ns_node_names.empty())
        {
            RCLCPP_WARN_STREAM(*logger_, "No nodes found in the ROS graph.");
            return;
        }
        for (const std::string &node_name : ns_node_names)
        {
            QTreeWidgetItem *node_item = new QTreeWidgetItem(ui_->listNodeParam);
            node_item->setText(0, QString::fromStdString(node_name));
            node_item->setCheckState(0, Qt::Unchecked);

            if (node_name == nh_->get_node_base_interface()->get_fully_qualified_name())
            {
                auto names = nh_->list_parameters({}, 0).names;
                for (const auto& param_name : names) {
                    QStringList parts = QString::fromStdString(param_name).split('.');
                    QTreeWidgetItem *current_parent = node_item;
                    for (const auto& part : parts)
                    {
                        current_parent = getOrCreateChild(current_parent, part);
                    }
                }
            }
            else
            {
                auto service_name = node_name + "/list_parameters";
                auto client = nh_->create_client<rcl_interfaces::srv::ListParameters>(service_name);

                list_params_clients_.push_back(client);

                if (!client->wait_for_service(std::chrono::milliseconds(50)))
                {
                    node_item->setDisabled(true);
                    // node_item->setText(0, node_item->text(0) + " (Service Offline)");
                    RCLCPP_WARN_STREAM(*logger_, "Service " << service_name << " not available for node: " << node_name);
                    continue;
                }

                auto request = std::make_shared<rcl_interfaces::srv::ListParameters::Request>();
                request->depth = 0;

                client->async_send_request(request, [this, node_item, client](rclcpp::Client<rcl_interfaces::srv::ListParameters>::SharedFuture future) {
                    try
                    {
                        auto response = future.get();

                        // QtのメインスレッドでUIを更新（スレッドセーフのため）
                        QMetaObject::invokeMethod(this, [this, node_item, client, names = response->result.names](){
                            for (const auto& param_name : names) {
                                QStringList parts = QString::fromStdString(param_name).split('.');
                                QTreeWidgetItem *current_parent = node_item;
                                for (const auto& part : parts)
                                {
                                    current_parent = getOrCreateChild(current_parent, part);
                                }
                            }
                            list_params_clients_.erase(std::remove(list_params_clients_.begin(), list_params_clients_.end(), client), list_params_clients_.end());
                        });
                    }
                    catch (const std::exception &e)
                    {
                        RCLCPP_ERROR_STREAM(*logger_, "Failed to list parameters: " << e.what());
                    }
                });
            }
        }
    }

    void ParamDialog::onItemChanged(QTreeWidgetItem *item, int column)
    {
        if (column != 0)
            return;

        ui_->listNodeParam->blockSignals(true);
        Qt::CheckState state = item->checkState(0);
        updateChildCheckState(item, state);
        updateParentCheckState(item);
        ui_->listNodeParam->blockSignals(false);
    }

    void ParamDialog::updateChildCheckState(QTreeWidgetItem *item, Qt::CheckState state)
    {
        for (int i = 0; i < item->childCount(); ++i)
        {
            item->child(i)->setCheckState(0, state);
            updateChildCheckState(item->child(i), state);
        }
    }

    void ParamDialog::updateParentCheckState(QTreeWidgetItem *item)
    {
        QTreeWidgetItem *parent = item->parent();
        if (!parent)
            return;

        int checked_count = 0;
        for (int i = 0; i < parent->childCount(); ++i)
        {
            if (parent->child(i)->checkState(0) == Qt::Checked)
            {
                checked_count++;
            }
        }

        if (checked_count == 0)
        {
            parent->setCheckState(0, Qt::Unchecked);
        }
        else if (checked_count == parent->childCount())
        {
            parent->setCheckState(0, Qt::Checked);
        }
        else
        {
            parent->setCheckState(0, Qt::PartiallyChecked); // 中間状態
        }

        updateParentCheckState(parent); // さらに上の親へ
    }

    QTreeWidgetItem *ParamDialog::getOrCreateChild(QTreeWidgetItem *parent, const QString &name)
    {
        for (int i = 0; i < parent->childCount(); ++i)
        {
            if (parent->child(i)->text(0) == name)
            {
                return parent->child(i);
            }
        }
        QTreeWidgetItem *new_child = new QTreeWidgetItem(parent);
        new_child->setText(0, name);
        new_child->setCheckState(0, Qt::Unchecked);
        return new_child;
    }

} // namespace rviz2_reconfigure

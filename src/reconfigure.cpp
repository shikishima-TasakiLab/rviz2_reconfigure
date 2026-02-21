#include "rviz2_reconfigure/reconfigure.hpp"

namespace rviz2_reconfigure
{
    RViz2Reconfigure::RViz2Reconfigure(QWidget *parent)
        : rviz_common::Panel(parent), ui_(new Ui::Reconfigure())
    {
        ui_->setupUi(this);
        ui_->listNodeParamValue->sortByColumn(0, Qt::AscendingOrder);

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
        ParamDialog dialog(nh_, this);
        if (dialog.exec() == QDialog::Accepted)
        {
            QList<QPair<QString, QString>> checked_params = dialog.getCheckedParams();

            for (const auto &param : checked_params)
            {
                RCLCPP_INFO_STREAM(nh_->get_logger(), "Selected Node: " << param.first.toStdString() << ", Param: " << param.second.toStdString());

                QString node_name = param.first;
                QString full_path = param.second;

                // 1. ノード名のトップレベルアイテムを探す（なければ作成）
                QTreeWidgetItem* node_item = nullptr;
                for (int i = 0; i < ui_->listNodeParamValue->topLevelItemCount(); ++i) {
                    if (ui_->listNodeParamValue->topLevelItem(i)->text(0) == node_name) {
                        node_item = ui_->listNodeParamValue->topLevelItem(i);
                        break;
                    }
                }
                if (!node_item) {
                    node_item = new QTreeWidgetItem(ui_->listNodeParamValue, {node_name});
                }

                // 2. パラメータの階層をドットで分割して展開
                QStringList parts = full_path.split('.');
                QTreeWidgetItem* current_parent = node_item;

                for (int i = 0; i < parts.size(); ++i) {
                    current_parent = getOrCreateChild(current_parent, parts[i]);
                    
                    // 3. 最後の要素（葉ノード）だけ編集可能にし、フルパスを隠しデータとして持たせる
                    if (i == parts.size() - 1) {
                        current_parent->setFlags(current_parent->flags() | Qt::ItemIsEditable);
                        // 後でSetParametersするために、このアイテムがどのノードのどのパスか保存しておく
                        current_parent->setData(0, Qt::UserRole, node_name);
                        current_parent->setData(1, Qt::UserRole, full_path);
                        current_parent->setText(1, "---"); // 2列目を値の表示用にする
                    }
                }

            // 追加後に全パラメータの最新値を一括取得する
            // refreshAllValues();
            }
        }
    }

    QTreeWidgetItem* RViz2Reconfigure::getOrCreateChild(QTreeWidgetItem *parent, const QString &name)
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
        return new_child;
    }

    ParamDialog::ParamDialog(rclcpp::Node::SharedPtr node_handle, rviz_common::Panel *parent)
        : QDialog(parent), nh_(node_handle), ui_(new Ui::ParamDialog())
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

    void ParamDialog::accept()
    {
        QDialog::accept();
    }

    void ParamDialog::refresh()
    {
        ui_->listNodeParam->clear();
        list_params_clients_.clear();

        RCLCPP_INFO_STREAM(nh_->get_logger(), nh_->get_node_base_interface()->get_fully_qualified_name());

        std::vector<std::string> ns_node_names = nh_->get_node_names();
        if (ns_node_names.empty())
        {
            RCLCPP_INFO_STREAM(nh_->get_logger(), "No nodes found in the ROS graph.");
            return;
        }
        for (const std::string &node_name : ns_node_names)
        {
            QTreeWidgetItem *node_item = new QTreeWidgetItem(ui_->listNodeParam);
            node_item->setText(0, QString::fromStdString(node_name));
            node_item->setCheckState(0, Qt::Unchecked);

            if (node_name == nh_->get_node_base_interface()->get_fully_qualified_name())
            {
                RCLCPP_INFO_STREAM(nh_->get_logger(), "Node: " << node_name << " is the current node.");
                auto names = nh_->list_parameters({}, 0).names;
                for (const auto& param_name : names) {
                    RCLCPP_INFO_STREAM(nh_->get_logger(), "Found parameter: " << param_name);
                    QStringList parts = QString::fromStdString(param_name).split('.');
                    QTreeWidgetItem *current_parent = node_item;
                    for (int i = 0; i < parts.size() - 1; ++i)
                    {
                        current_parent = getOrCreateChild(current_parent, parts[i]);
                    }
                    QTreeWidgetItem *param_item = new QTreeWidgetItem(current_parent);
                    param_item->setText(0, parts.last());
                    param_item->setCheckState(0, Qt::Unchecked);
                }
            }
            else
            {
                RCLCPP_INFO_STREAM(nh_->get_logger(), "Node: " << node_name);

                auto service_name = node_name + "/list_parameters";
                auto client = nh_->create_client<rcl_interfaces::srv::ListParameters>(service_name);

                list_params_clients_.push_back(client);

                if (!client->wait_for_service(std::chrono::milliseconds(50)))
                {
                    node_item->setDisabled(true);
                    // node_item->setText(0, node_item->text(0) + " (Service Offline)");
                    RCLCPP_WARN_STREAM(nh_->get_logger(), "Service " << service_name << " not available for node: " << node_name);
                    continue;
                }

                auto request = std::make_shared<rcl_interfaces::srv::ListParameters::Request>();
                request->depth = 0;

                client->async_send_request(request, [this, node_item](rclcpp::Client<rcl_interfaces::srv::ListParameters>::SharedFuture future) {
                    try
                    {
                        auto response = future.get();
                        RCLCPP_INFO_STREAM(nh_->get_logger(), "Received response for node: " << node_item->text(0).toStdString());
                        // QtのメインスレッドでUIを更新（スレッドセーフのため）
                        QMetaObject::invokeMethod(this, [this, node_item, names = response->result.names](){
                            for (const auto& param_name : names) {
                                RCLCPP_INFO_STREAM(nh_->get_logger(), "Found parameter: " << param_name);
                                QStringList parts = QString::fromStdString(param_name).split('.');
                                QTreeWidgetItem *current_parent = node_item;
                                for (const auto& part : parts)
                                {
                                    current_parent = getOrCreateChild(current_parent, part);
                                }
                                // // QTreeWidgetItem *param_item = new QTreeWidgetItem(current_parent);
                                // QTreeWidgetItem *param_item = current_parent;
                                // param_item->setText(0, parts.last());
                                // param_item->setCheckState(0, Qt::Unchecked);
                            }
                        });
                    }
                    catch (const std::exception &e)
                    {
                        RCLCPP_ERROR(nh_->get_logger(), "Failed to list parameters: %s", e.what());
                    }
                });
            }
        }

        RCLCPP_INFO_STREAM(nh_->get_logger(), "Finished refreshing node and parameter list.");
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

} // namespace rviz2_reconfigure

#include <pluginlib/class_list_macros.hpp>
PLUGINLIB_EXPORT_CLASS(rviz2_reconfigure::RViz2Reconfigure, rviz_common::Panel)

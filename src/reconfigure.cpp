#include "rviz2_reconfigure/reconfigure.hpp"

namespace rviz2_reconfigure
{
    RViz2Reconfigure::RViz2Reconfigure(QWidget *parent)
        : rviz_common::Panel(parent), ui_(new Ui::Reconfigure())
    {
        ui_->setupUi(this);
        ui_->listNodeParamValue->sortByColumn(0, Qt::AscendingOrder);

        connect(ui_->addPushBtn, &QPushButton::clicked, this, &RViz2Reconfigure::addPushBtn__clicked);
        connect(ui_->listNodeParamValue, &QTreeWidget::itemChanged, this, &RViz2Reconfigure::onItemChanged);
        connect(ui_->refreshPushBtn, &QPushButton::clicked, this, &RViz2Reconfigure::refreshAllValues);
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

            ui_->listNodeParamValue->blockSignals(true); // 追加処理中はシグナルをブロックしてonItemChangedが呼ばれないようにする

            for (const auto &param : checked_params)
            {
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
                        current_parent->setData(0, UserRole::FullPathRole, node_name);
                        current_parent->setData(1, UserRole::FullPathRole, full_path);
                        current_parent->setText(1, "---"); // 2列目を値の表示用にする
                    }
                }

            }
            ui_->listNodeParamValue->blockSignals(false); // シグナルのブロックを解除

            // 追加後に全パラメータの最新値を一括取得する
            refreshAllValues();
        }
    }

    void RViz2Reconfigure::refreshAllValues()
    {
        // すべてのノードとパラメータのアイテムを収集
        QList<QTreeWidgetItem*> leaf_items;
        for (int i = 0; i < ui_->listNodeParamValue->topLevelItemCount(); ++i) {
            collectLeafItems(ui_->listNodeParamValue->topLevelItem(i), leaf_items);
        }

        // 2. ノード名ごとにパラメータパスとアイテムを整理
        // map<ノード名, pair<パスのリスト, アイテムのリスト>>
        std::map<std::string, std::pair<std::vector<std::string>, std::vector<QTreeWidgetItem*>>> batch_map;

        for (QTreeWidgetItem* item : leaf_items) {
            std::string node_name = item->data(0, Qt::UserRole).toString().toStdString();
            std::string full_path = item->data(1, Qt::UserRole).toString().toStdString();

            if (node_name.empty() || full_path.empty()) continue;

            batch_map[node_name].first.push_back(full_path);
            batch_map[node_name].second.push_back(item);
        }

        // 3. 各ノードに対して非同期リクエストを送信
        for (const auto& [node_name, data] : batch_map) {
            auto client = nh_->create_client<rcl_interfaces::srv::GetParameters>(node_name + "/get_parameters");

            if (!client->wait_for_service(std::chrono::milliseconds(100))) continue;
            get_params_clients_.push_back(client);

            auto request = std::make_shared<rcl_interfaces::srv::GetParameters::Request>();
            request->names = data.first;

            auto callback = [this, items = data.second, client](rclcpp::Client<rcl_interfaces::srv::GetParameters>::SharedFuture future) {
                try {
                    auto response = future.get();
                    QMetaObject::invokeMethod(this, [this, items, response](){
                        // アイテムの更新中はシグナルをブロックしてonItemChangedが呼ばれないようにする
                        ui_->listNodeParamValue->blockSignals(true);
                        for (size_t i = 0; i < items.size(); ++i) {
                            const auto& param_value = response->values[i];
                            QString value_str;

                            // 値の型に応じて表示形式を変える
                            switch (param_value.type) {
                                case rclcpp::ParameterType::PARAMETER_BOOL:
                                    value_str = param_value.bool_value ? "true" : "false";
                                    break;
                                case rclcpp::ParameterType::PARAMETER_INTEGER:
                                    value_str = QString::number(param_value.integer_value);
                                    break;
                                case rclcpp::ParameterType::PARAMETER_DOUBLE:
                                    value_str = QString::number(param_value.double_value);
                                    break;
                                case rclcpp::ParameterType::PARAMETER_STRING:
                                    value_str = QString::fromStdString(param_value.string_value);
                                    break;
                                default:
                                    value_str = "<unsupported type>";
                            }
                            items[i]->setData(0, UserRole::ParamTypeRole, QVariant::fromValue(param_value.type)); // 型情報も保存しておく
                            items[i]->setText(1, value_str); // 2列目に値を表示
                        }
                        // シグナルのブロックを解除
                        ui_->listNodeParamValue->blockSignals(false);
                    });
                } catch (const std::exception &e) {
                    RCLCPP_ERROR_STREAM(nh_->get_logger(), "Failed to get parameter values: " << e.what());
                }
                // コールバックが終わったらクライアントを削除してリストからも消す
                get_params_clients_.erase(std::remove(get_params_clients_.begin(), get_params_clients_.end(), client), get_params_clients_.end());
            };

            client->async_send_request(request, callback);
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

    void RViz2Reconfigure::collectLeafItems(QTreeWidgetItem *parent, QList<QTreeWidgetItem*> &leaf_items)
    {
        for (int i = 0; i < parent->childCount(); ++i) {
            QTreeWidgetItem* child = parent->child(i);
            if (child->childCount() == 0) {
                // 子がいない＝葉ノード（パラメータ）
                leaf_items.append(child);
            } else {
                // 子がいる＝さらに深く探索
                collectLeafItems(child, leaf_items);
            }
        }    
    }


    void RViz2Reconfigure::onItemChanged(QTreeWidgetItem *item, int column)
    {
        if (column != 1) return; // 値の列だけ処理

        QString node_name = item->data(0, UserRole::FullPathRole).toString();
        QString full_path = item->data(1, UserRole::FullPathRole).toString();
        int param_type = item->data(0, UserRole::ParamTypeRole).toInt();
        QString new_value_str = item->text(1);

        if (node_name.isEmpty() || full_path.isEmpty()) return;

        auto client = nh_->create_client<rcl_interfaces::srv::SetParameters>(node_name.toStdString() + "/set_parameters");
        if (!client->wait_for_service(std::chrono::milliseconds(50))) {
            RCLCPP_ERROR_STREAM(nh_->get_logger(), "Service " << node_name.toStdString() << "/set_parameters not available");
            return;
        }
        set_params_clients_.push_back(client);

        auto request = std::make_shared<rcl_interfaces::srv::SetParameters::Request>();
        rcl_interfaces::msg::Parameter param;
        param.name = full_path.toStdString();
        param.value.type = static_cast<rclcpp::ParameterType>(param_type);

        try {
            switch (param_type) {
                case rclcpp::ParameterType::PARAMETER_BOOL:
                    param.value.bool_value = (new_value_str.toLower() == "true" || new_value_str == "1");
                    break;
                case rclcpp::ParameterType::PARAMETER_INTEGER:
                    param.value.integer_value = new_value_str.toLongLong();
                    break;
                case rclcpp::ParameterType::PARAMETER_DOUBLE:
                    param.value.double_value = new_value_str.toDouble();
                    break;
                case rclcpp::ParameterType::PARAMETER_STRING:
                    param.value.string_value = new_value_str.toStdString();
                    break;
                default:
                    RCLCPP_WARN_STREAM(nh_->get_logger(), "Unsupported parameter type for setting value: " << param_type);
                    return;
            }
        } catch (const std::exception &e) {
            RCLCPP_ERROR_STREAM(nh_->get_logger(), "Failed to parse new value: " << e.what());
            return;
        }

        request->parameters.push_back(param);

        client->async_send_request(request, [this, client, node_name, full_path](rclcpp::Client<rcl_interfaces::srv::SetParameters>::SharedFuture future) {
            try {
                auto response = future.get();
                if (response->results.empty() || !response->results[0].successful) {
                    throw std::runtime_error(response->results.empty() ? "Unknown" : response->results[0].reason);
                }
                RCLCPP_INFO_STREAM(nh_->get_logger(), "Successfully set [" << full_path.toStdString() << "]");
            } catch (const std::exception &e) {
                RCLCPP_ERROR_STREAM(nh_->get_logger(), "Failed to set [" << full_path.toStdString() << "]: " << e.what());
                // 失敗した場合は、再取得して元の値に戻す処理を呼ぶのが望ましい
                QMetaObject::invokeMethod(this, &RViz2Reconfigure::refreshAllValues);
            }
            // コールバックが終わったらクライアントを削除してリストからも消す
            set_params_clients_.erase(std::remove(set_params_clients_.begin(), set_params_clients_.end(), client), set_params_clients_.end());
        });
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

    void ParamDialog::refresh()
    {
        ui_->listNodeParam->clear();
        list_params_clients_.clear();

        std::vector<std::string> ns_node_names = nh_->get_node_names();
        if (ns_node_names.empty())
        {
            RCLCPP_WARN_STREAM(nh_->get_logger(), "No nodes found in the ROS graph.");
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
                    RCLCPP_WARN_STREAM(nh_->get_logger(), "Service " << service_name << " not available for node: " << node_name);
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
                        RCLCPP_ERROR(nh_->get_logger(), "Failed to list parameters: %s", e.what());
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

#include <pluginlib/class_list_macros.hpp>
PLUGINLIB_EXPORT_CLASS(rviz2_reconfigure::RViz2Reconfigure, rviz_common::Panel)

#include "rviz2_reconfigure/reconfigure.hpp"

namespace rviz2_reconfigure
{
    RViz2Reconfigure::RViz2Reconfigure(QWidget *parent)
        : rviz_common::Panel(parent), ui_(new Ui::Reconfigure())
    {
        ui_->setupUi(this);
        ui_->listNodeParamValue->sortByColumn(0, Qt::AscendingOrder);
        ui_->listNodeParamValue->setItemDelegateForColumn(1, new ParamEditorDelegate(this)); // 2列目の編集はParamEditorDelegateに任せる

        connect(ui_->addPushBtn, &QPushButton::clicked, this, &RViz2Reconfigure::addPushBtn__clicked);
        connect(ui_->listNodeParamValue, &QTreeWidget::itemChanged, this, &RViz2Reconfigure::onItemChanged);
        connect(ui_->refreshPushBtn, &QPushButton::clicked, this, &RViz2Reconfigure::refreshAllValues);
        connect(ui_->autoRefreshChkBox, &QCheckBox::stateChanged, this, &RViz2Reconfigure::autoRefreshChkBox__CheckStateChanged);
        // connect(ui_->autoRefreshChkBox, &QCheckBox::checkStateChanged, this, &RViz2Reconfigure::autoRefreshChkBox__CheckStateChanged); since Qt 6.7
        connect(ui_->removePushBtn, &QPushButton::clicked, this, &RViz2Reconfigure::removePushBtn__clicked);
        connect(ui_->importPushBtn, &QPushButton::clicked, this, &RViz2Reconfigure::importPushBtn__clicked);
        connect(ui_->exportPushBtn, &QPushButton::clicked, this, &RViz2Reconfigure::exportPushBtn__clicked);
    }

    RViz2Reconfigure::~RViz2Reconfigure()
    {
    }

    void RViz2Reconfigure::onInitialize()
    {
        nh_ = getDisplayContext()->getRosNodeAbstraction().lock()->get_raw_node();
        logger_ = std::make_shared<rclcpp::Logger>(nh_->get_logger().get_child(getName().toStdString()));
    }

    void RViz2Reconfigure::load(const rviz_common::Config &config)
    {
        rviz_common::Panel::load(config);

        rviz_common::Config refresh_config = config.mapGetChild("refresh");
        if (refresh_config.isValid() && refresh_config.getType() == rviz_common::Config::Map) {
            bool auto_refresh = refresh_config.mapGetChild("auto_refresh").getType() == rviz_common::Config::Value
                                    ? refresh_config.mapGetChild("auto_refresh").getValue().toBool()
                                    : false;
            double refresh_interval = refresh_config.mapGetChild("refresh_interval").getType() == rviz_common::Config::Value
                                            ? refresh_config.mapGetChild("refresh_interval").getValue().toDouble()
                                            : 5.0;

            ui_->autoRefreshChkBox->setChecked(auto_refresh);
            ui_->autoRefreshSpinBox->setValue(refresh_interval);
        }

        rviz_common::Config params_config = config.mapGetChild("selected_params");
        if (params_config.isValid() && params_config.getType() == rviz_common::Config::List) {
            QList<QPair<QString, QString>> params_to_load;

            size_t num_items = params_config.listLength();
            for (size_t i = 0; i < num_items; ++i) {
                rviz_common::Config item_config = params_config.listChildAt(i);
                if (!item_config.isValid() || item_config.getType() != rviz_common::Config::Map) {
                    continue; // アイテムの形式が不正な場合はスキップ
                }
                QString node_name = item_config.mapGetChild("node_name").getType() == rviz_common::Config::Value
                                        ? item_config.mapGetChild("node_name").getValue().toString()
                                        : QString();
                QString full_path = item_config.mapGetChild("full_path").getType() == rviz_common::Config::Value
                                        ? item_config.mapGetChild("full_path").getValue().toString()
                                        : QString();
                if (node_name.isEmpty() || full_path.isEmpty()) continue; // ノード名やフルパスが空の場合はスキップ
                params_to_load.append(QPair<QString, QString>(node_name, full_path));
            }

            ui_->listNodeParamValue->clear();
            loadParamsToTree(params_to_load);
        }

    }

    void RViz2Reconfigure::save(rviz_common::Config config) const
    {
        rviz_common::Panel::save(config);

        rviz_common::Config refresh_config = config.mapMakeChild("refresh");
        refresh_config.mapSetValue("auto_refresh", ui_->autoRefreshChkBox->isChecked());
        refresh_config.mapSetValue("refresh_interval", ui_->autoRefreshSpinBox->value());

        rviz_common::Config params_config = config.mapMakeChild("selected_params");

        QList<QTreeWidgetItem*> leaf_items;
        collectLeafItems(ui_->listNodeParamValue->invisibleRootItem(), leaf_items);

        for (QTreeWidgetItem* item : leaf_items) {
            QString node_name = item->data(0, UserRole::FullPathRole).toString();
            QString full_path = item->data(1, UserRole::FullPathRole).toString();

            if (node_name.isEmpty() || full_path.isEmpty()) continue;

            rviz_common::Config item_config = params_config.listAppendNew();

            item_config.mapSetValue("node_name", node_name);
            item_config.mapSetValue("full_path", full_path);
        }
    }

    void RViz2Reconfigure::addPushBtn__clicked()
    {
        ParamDialog dialog(nh_, logger_, this);
        if (dialog.exec() == QDialog::Accepted)
        {
            loadParamsToTree(dialog.getCheckedParams());
        }
    }

    void RViz2Reconfigure::refreshAllValues()
    {
        // すべてのノードとパラメータのアイテムを収集
        QList<QTreeWidgetItem*> leaf_items;
        collectLeafItems(ui_->listNodeParamValue->invisibleRootItem(), leaf_items);

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

                            // 値の型に応じて表示形式を変える
                            switch (param_value.type) {
                                case rclcpp::ParameterType::PARAMETER_BOOL:
                                    items[i]->setText(1, ""); // 2列目に値を表示
                                    items[i]->setCheckState(1, param_value.bool_value ? Qt::Checked : Qt::Unchecked); // チェックボックスで表示
                                    items[i]->setFlags(items[i]->flags() & ~Qt::ItemIsEditable); // 編集不可にする
                                    items[i]->setFlags(items[i]->flags() | Qt::ItemIsUserCheckable); // チェックボックスを有効にする
                                    break;
                                case rclcpp::ParameterType::PARAMETER_INTEGER:
                                    items[i]->setText(1, QString::number(param_value.integer_value)); // 2列目に値を表示
                                    break;
                                case rclcpp::ParameterType::PARAMETER_DOUBLE:
                                    items[i]->setText(1, QString::number(param_value.double_value)); // 2列目に値を表示
                                    break;
                                case rclcpp::ParameterType::PARAMETER_STRING:
                                    items[i]->setText(1, QString::fromStdString(param_value.string_value)); // 2列目に値を表示
                                    break;
                                default:
                                    items[i]->setText(1, "<unsupported type>"); // 2列目に値を表示
                            }
                            items[i]->setData(1, UserRole::ParamTypeRole, QVariant::fromValue(param_value.type)); // 型情報も保存しておく
                        }
                    });
                } catch (const std::exception &e) {
                    RCLCPP_ERROR_STREAM(*logger_, "Failed to get parameter values: " << e.what());
                }
                // シグナルのブロックを解除
                ui_->listNodeParamValue->blockSignals(false);
                // コールバックが終わったらクライアントを削除してリストからも消す
                get_params_clients_.erase(std::remove(get_params_clients_.begin(), get_params_clients_.end(), client), get_params_clients_.end());
            };

            client->async_send_request(request, callback);
        }
    }

    void RViz2Reconfigure::onItemChanged(QTreeWidgetItem *item, int column)
    {
        if (column != 1) return; // 値の列だけ処理

        QString node_name = item->data(0, UserRole::FullPathRole).toString();
        QString full_path = item->data(1, UserRole::FullPathRole).toString();

        if (node_name.isEmpty() || full_path.isEmpty()) return;

        std::map<std::string, std::vector<std::pair<std::string, QTreeWidgetItem*>>> batch_map;
        batch_map[node_name.toStdString()].push_back({full_path.toStdString(), item});

        setParamValues(batch_map);
    }

    void RViz2Reconfigure::autoRefreshChkBox__CheckStateChanged(int state)
    {
        switch (state) {
            case Qt::Checked:
                ui_->autoRefreshSpinBox->setEnabled(false);
                auto_refresh_timer_ = nh_->create_wall_timer(
                    std::chrono::milliseconds(static_cast<int>(ui_->autoRefreshSpinBox->value() * 1000.0)),
                    std::bind(&RViz2Reconfigure::refreshAllValues, this)
                );
                break;
            case Qt::Unchecked:
                if (auto_refresh_timer_) {
                    auto_refresh_timer_->cancel();
                    auto_refresh_timer_.reset();
                }
                ui_->autoRefreshSpinBox->setEnabled(true);
                break;
            default:
                ui_->autoRefreshSpinBox->setEnabled(true);
                break;
        }
    }

    void RViz2Reconfigure::removePushBtn__clicked()
    {
        QList<QTreeWidgetItem*> selected_items = ui_->listNodeParamValue->selectedItems();

        if (selected_items.isEmpty()) return;

        for (QTreeWidgetItem* item : selected_items) {
            QTreeWidgetItem* parent = item->parent();
            if (parent) {
                // 中間階層やノード配下の場合
                parent->removeChild(item);
            } else {
                // トップレベル（ノード名）の場合
                int index = ui_->listNodeParamValue->indexOfTopLevelItem(item);
                if (index != -1) {
                    ui_->listNodeParamValue->takeTopLevelItem(index);
                }
            }
            delete item; // アイテムを削除してメモリも解放

            checkAndRemoveEmptyParents(parent); // 親が空になったら再帰的に削除していく
        }
    }

    void RViz2Reconfigure::importPushBtn__clicked()
    {
        QString file_path = QFileDialog::getOpenFileName(this, "Import Parameters", "", "YAML Files (*.yaml *.yml);;All Files (*)");
        if (file_path.isEmpty()) return;

        std::map<std::string, std::map<std::string, QTreeWidgetItem*>> registered;
        {
            QList<QTreeWidgetItem*> leaf_items;
            collectLeafItems(ui_->listNodeParamValue->invisibleRootItem(), leaf_items);

            for (QTreeWidgetItem* item : leaf_items) {
                std::string node_name = item->data(0, UserRole::FullPathRole).toString().toStdString();
                std::string full_path = item->data(1, UserRole::FullPathRole).toString().toStdString();
                if (node_name.empty() || full_path.empty()) continue;
                registered[node_name][full_path] = item;
            }
        }

        std::map<std::string, std::vector<std::pair<std::string, QTreeWidgetItem*>>> batch_map;
        ui_->listNodeParamValue->blockSignals(true);
        try {
            const YAML::Node &ros_nodes = YAML::LoadFile(file_path.toStdString());
            if (!ros_nodes.IsMap()) {
                throw std::runtime_error("Invalid YAML format: top-level structure must be a map of node names to parameter lists");
            }

            for (const auto& ros_node : ros_nodes) {
                std::string node_name = "/" + ros_node.first.as<std::string>();
                const YAML::Node &ros_params = ros_node.second["ros__parameters"];

                if (ros_params) {
                    parseYamlRecursive(node_name, ros_params, "", registered, batch_map);
                } else {
                    RCLCPP_WARN_STREAM(*logger_, "Node " << node_name << " has no ros__parameters section, skipping");
                }
            }
        } catch (const std::exception &e) {
            RCLCPP_ERROR_STREAM(*logger_, "Failed to load YAML file: " << e.what());
            ui_->listNodeParamValue->blockSignals(false);
            return;
        }

        setParamValues(batch_map);
        ui_->listNodeParamValue->blockSignals(false);
    }

    void RViz2Reconfigure::exportPushBtn__clicked()
    {
        QString file_path = QFileDialog::getSaveFileName(this, "Export Parameters", "", "YAML Files (*.yaml *.yml);;All Files (*)");
        if (file_path.isEmpty()) return;

        QList<QTreeWidgetItem*> leaf_items;
        collectLeafItems(ui_->listNodeParamValue->invisibleRootItem(), leaf_items);

        if (leaf_items.isEmpty()) {
            RCLCPP_WARN_STREAM(*logger_, "No parameters to export");
            return;
        }

        YAML::Emitter out;
        out << YAML::BeginMap;

        for (int i = 0; i < ui_->listNodeParamValue->topLevelItemCount(); i++) {
            QTreeWidgetItem* node_item = ui_->listNodeParamValue->topLevelItem(i);
            std::string node_full = node_item->text(0).toStdString();
            std::string node_name = (node_full[0] == '/') ? node_full.substr(1) : node_full;

            out << YAML::Key << node_name;
            out << YAML::Value << YAML::BeginMap;
            out << YAML::Key << "ros__parameters";
            out << YAML::Value << YAML::BeginMap;

            for (int j = 0; j < node_item->childCount(); ++j) {
                serializeItem(node_item->child(j), out);
            }

            out << YAML::EndMap; // ros__parameters
            out << YAML::EndMap; // node_name
        }

        out << YAML::EndMap; // Entire

        try {
            std::ofstream fout(file_path.toStdString());
            fout << out.c_str();
            fout.close();
            RCLCPP_INFO_STREAM(*logger_, "Parameters exported successfully to " << file_path.toStdString());
        } catch (const std::exception &e) {
            RCLCPP_ERROR_STREAM(*logger_, "Failed to save YAML file: " << e.what());
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

    void RViz2Reconfigure::collectLeafItems(QTreeWidgetItem *parent, QList<QTreeWidgetItem*> &leaf_items) const
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

    void RViz2Reconfigure::loadParamsToTree(const QList<QPair<QString, QString>> &params_to_load)
    {
        ui_->listNodeParamValue->blockSignals(true); // 追加処理中はシグナルをブロックしてonItemChangedが呼ばれないようにする

        for (const auto &param : params_to_load)
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

    void RViz2Reconfigure::checkAndRemoveEmptyParents(QTreeWidgetItem *parent)
    {
        if (!parent) return;

        if (parent->childCount() == 0) {
            QTreeWidgetItem* grandparent = parent->parent();
            if (grandparent) {
                grandparent->removeChild(parent);
                delete parent; // メモリも解放
                checkAndRemoveEmptyParents(grandparent); // 再帰的に上の階層もチェック
            } else {
                // トップレベルアイテムの場合は直接削除
                int index = ui_->listNodeParamValue->indexOfTopLevelItem(parent);
                if (index != -1) {
                    ui_->listNodeParamValue->takeTopLevelItem(index);
                    delete parent; // メモリも解放
                }
            }
        }
    }

    void RViz2Reconfigure::setParamValues(const std::map<std::string, std::vector<std::pair<std::string, QTreeWidgetItem*>>> &batch_map)
    {
        for (const auto& [node_name, data] : batch_map) {
            auto client = nh_->create_client<rcl_interfaces::srv::SetParameters>(node_name + "/set_parameters");

            if (!client->wait_for_service(std::chrono::milliseconds(50))) {
                RCLCPP_ERROR_STREAM(*logger_, "Service " << node_name << "/set_parameters not available");
                continue;
            }
            set_params_clients_.push_back(client);

            auto request = std::make_shared<rcl_interfaces::srv::SetParameters::Request>();
            std::vector<QString> list_value_strs;
            
            for (const auto& [full_path, item] : data) {
                int param_type = item->data(1, UserRole::ParamTypeRole).toInt();
                QString new_value_str = item->text(1);

                rcl_interfaces::msg::Parameter param;
                param.name = full_path;
                param.value.type = static_cast<rclcpp::ParameterType>(param_type);

                try {
                    switch (param_type) {
                        case rclcpp::ParameterType::PARAMETER_BOOL:
                            param.value.bool_value = (item->checkState(1) == Qt::Checked);
                            new_value_str = param.value.bool_value ? "true" : "false"; // 表示用に文字列も更新
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
                            RCLCPP_WARN_STREAM(*logger_, "Unsupported parameter type for setting value: " << param_type);
                            continue; // このパラメータはスキップ
                    }
                    request->parameters.push_back(param);
                    list_value_strs.push_back(new_value_str);
                } catch (const std::exception &e) {
                    RCLCPP_ERROR_STREAM(*logger_, "Failed to parse new value for [" << full_path << "]: " << e.what());
                    continue; // このパラメータはスキップ
                }
            }

            client->async_send_request(request, [this, client, request, list_value_strs](rclcpp::Client<rcl_interfaces::srv::SetParameters>::SharedFuture future) {
                try {
                    auto response = future.get();
                    // 成功/失敗の処理
                    if (response->results.empty() || !response->results[0].successful) {
                        throw std::runtime_error(response->results.empty() ? "Unknown" : response->results[0].reason);
                    }
                    for (size_t i = 0; i < request->parameters.size(); ++i) {
                        RCLCPP_INFO_STREAM(*logger_, "Successfully set [" << request->parameters[i].name << "]: " << list_value_strs[i].toStdString());
                    }
                } catch (const std::exception &e) {
                    RCLCPP_ERROR_STREAM(*logger_, "Failed to set parameter values: " << e.what());
                }
                // コールバックが終わったらクライアントを削除してリストからも消す
                set_params_clients_.erase(std::remove(set_params_clients_.begin(), set_params_clients_.end(), client), set_params_clients_.end());
                if (set_params_clients_.empty()) {
                    // すべてのリクエストが終わったら、最新値を再取得してUIを更新する
                    QMetaObject::invokeMethod(this, &RViz2Reconfigure::refreshAllValues);
                }
            });
        }
    }

    void RViz2Reconfigure::parseYamlRecursive(
        const std::string &node_name,
        const YAML::Node &current_node,
        const std::string &prefix,
        std::map<std::string, std::map<std::string, QTreeWidgetItem*>> &registered,
        std::map<std::string, std::vector<std::pair<std::string, QTreeWidgetItem*>>> &batch_map
    )
    {
        for (const auto& item : current_node) {
            std::string key = item.first.as<std::string>();
            std::string full_path = prefix.empty() ? key : prefix + "." + key;

            if (item.second.IsMap()) {
                // ノード（辞書）なら再帰的に探索
                parseYamlRecursive(node_name, item.second, full_path, registered, batch_map);
            } else {
                if (registered.count(node_name) && registered[node_name].count(full_path)) {
                    QTreeWidgetItem* existing_item = registered[node_name][full_path];
                    int param_type = existing_item->data(1, UserRole::ParamTypeRole).toInt();

                    // 既存アイテムの値を更新
                    switch (param_type)
                    {
                    case rclcpp::ParameterType::PARAMETER_BOOL:
                        existing_item->setCheckState(1, item.second.as<bool>() ? Qt::Checked : Qt::Unchecked);
                        break;
                    
                    case rclcpp::ParameterType::PARAMETER_INTEGER:
                        existing_item->setText(1, QString::number(item.second.as<long long>()));
                        break;

                    case rclcpp::ParameterType::PARAMETER_DOUBLE:
                        existing_item->setText(1, QString::number(item.second.as<double>()));
                        break;

                    case rclcpp::ParameterType::PARAMETER_STRING:
                        existing_item->setText(1, QString::fromStdString(item.second.as<std::string>()));
                        break;

                    default:
                        RCLCPP_WARN_STREAM(*logger_, "Unsupported parameter type for [" << full_path << "]");
                        break;
                    }

                    batch_map[node_name].push_back({full_path, existing_item}); // 既存アイテムをバッチ更新対象に追加
                }
            }
        }
    }

    void RViz2Reconfigure::serializeItem(QTreeWidgetItem *item, YAML::Emitter &out)
    {
        std::string key = item->text(0).toStdString();

        if (item->childCount() > 0) {
            out << YAML::Key << key;
            out << YAML::Value << YAML::BeginMap;
            for (int i = 0; i < item->childCount(); ++i) {
                serializeItem(item->child(i), out);
            }
            out << YAML::EndMap;
        } else {
            int param_type = item->data(1, UserRole::ParamTypeRole).toInt();
            QString value_str = item->text(1);

            out << YAML::Key << key;
            out << YAML::Value;

            switch (param_type) {
                case rclcpp::ParameterType::PARAMETER_BOOL:
                    out << (value_str.toLower() == "true" || value_str == "1" || item->checkState(1) == Qt::Checked);
                    break;
                case rclcpp::ParameterType::PARAMETER_INTEGER:
                    out << value_str.toLongLong();
                    break;
                case rclcpp::ParameterType::PARAMETER_DOUBLE:
                    out << value_str.toDouble();
                    break;
                case rclcpp::ParameterType::PARAMETER_STRING:
                    out << value_str.toStdString();
                    break;
                default:
                    RCLCPP_WARN_STREAM(*logger_, "Unsupported parameter type for export: " << item->data(0, UserRole::FullPathRole).toString().toStdString());
                    break;
            }
        }
    }


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


    QWidget* ParamEditorDelegate::createEditor(QWidget *parent, const QStyleOptionViewItem &option, const QModelIndex &index) const
    {
        QWidget* editor = QStyledItemDelegate::createEditor(parent, option, index);
        QLineEdit* line_edit = qobject_cast<QLineEdit*>(editor);

        if (line_edit) {
            int param_type = index.data(RViz2Reconfigure::ParamTypeRole).toInt();

            switch (param_type) {
                case rclcpp::ParameterType::PARAMETER_BOOL:
                {
                    line_edit->setReadOnly(true); // boolはチェックボックスで編集するのでテキスト編集は不可にする
                } break;
                case rclcpp::ParameterType::PARAMETER_INTEGER:
                {
                    line_edit->setValidator(new QIntValidator(line_edit));
                } break;
                case rclcpp::ParameterType::PARAMETER_DOUBLE:
                {
                    auto* varidator = new QDoubleValidator(line_edit);
                    varidator->setNotation(QDoubleValidator::StandardNotation);
                    line_edit->setValidator(varidator);
                } break;
                case rclcpp::ParameterType::PARAMETER_STRING:
                    // 文字列は特に制限なし
                    break;
                default:
                    line_edit->setReadOnly(true); // サポート外の型は編集不可にする
                    break;
            }
        }
        return editor;
    }

} // namespace rviz2_reconfigure

#include <pluginlib/class_list_macros.hpp>
PLUGINLIB_EXPORT_CLASS(rviz2_reconfigure::RViz2Reconfigure, rviz_common::Panel)

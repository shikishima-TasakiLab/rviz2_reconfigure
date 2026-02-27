#include "rviz2_reconfigure/delegate.hpp"
#include "rviz2_reconfigure/reconfigure.hpp"

namespace rviz2_reconfigure
{

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
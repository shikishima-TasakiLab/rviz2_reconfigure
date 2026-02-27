#ifndef RVIZ2_RECONFIGURE__DELEGATE_HPP_
#define RVIZ2_RECONFIGURE__DELEGATE_HPP_

#ifndef Q_MOC_RUN

#include <rclcpp/rclcpp.hpp>
#include <QLineEdit>
#include <QStyledItemDelegate>

#endif // Q_MOC_RUN

namespace rviz2_reconfigure
{

    class ParamEditorDelegate : public QStyledItemDelegate
    {
        Q_OBJECT
    public:
        ParamEditorDelegate(QObject *parent = nullptr) : QStyledItemDelegate(parent) {}
        QWidget* createEditor(QWidget *parent, const QStyleOptionViewItem &option, const QModelIndex &index) const override;
    };

} // namespace rviz2_reconfigure

#endif // RVIZ2_RECONFIGURE__PARAM_DIALOG_HPP_

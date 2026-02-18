#include "rviz2_reconfigure/reconfigure.hpp"

namespace rviz2_reconfigure
{
    RViz2Reconfigure::RViz2Reconfigure(QWidget *parent)
        : rviz_common::Panel(parent)
    {
    }

    RViz2Reconfigure::~RViz2Reconfigure()
    {
    }

    void RViz2Reconfigure::onInitialize()
    {
    }

    void RViz2Reconfigure::load(const rviz_common::Config &config)
    {
        rviz_common::Panel::load(config);
    }

    void RViz2Reconfigure::save(rviz_common::Config config) const
    {
        rviz_common::Panel::save(config);
    }

} // namespace rviz2_reconfigure

#include <pluginlib/class_list_macros.hpp>
PLUGINLIB_EXPORT_CLASS(rviz2_reconfigure::RViz2Reconfigure, rviz_common::Panel)

// SPDX-License-Identifier: BSD-3-Clause
// SPDX-FileCopyrightText: Czech Technical University in Prague
// SPDX-FileCopyrightText: Dorian Scholz, TU Darmstadt

#pragma once

#include <QMenuBar>
#include <QPointer>

#include <memory>
#include <string>

#if __has_include(<rqt_gui_cpp/plugin.hpp>)
#include <rqt_gui_cpp/plugin.hpp>
#else
#include <rqt_gui_cpp/plugin.h>
#endif
#include <rviz_common/ros_integration/ros_node_abstraction_iface.hpp>
#include <rviz_common/visualization_frame.hpp>

namespace rqt_rviz
{

namespace detail
{

struct RVizRosNodeOwner
{
  explicit RVizRosNodeOwner(
    const std::shared_ptr<rviz_common::ros_integration::RosNodeAbstractionIface> & ros_node);

  std::shared_ptr<rviz_common::ros_integration::RosNodeAbstractionIface> ros_node_;
};

std::shared_ptr<rviz_common::ros_integration::RosNodeAbstractionIface> createRosNodeAbstraction(
  const rclcpp::Node::SharedPtr & node, const std::string & sub_node_name);

}  // namespace detail

// This base order releases the node only after VisualizationFrame has been destroyed.
class RVizVisualizationFrame
  : private detail::RVizRosNodeOwner, public rviz_common::VisualizationFrame
{
public:
  explicit RVizVisualizationFrame(
    const std::shared_ptr<rviz_common::ros_integration::RosNodeAbstractionIface> & ros_node);

  bool prepareForConfiguration();

  void loadDisplayConfig(const QString & path);
};

class RViz : public rqt_gui_cpp::Plugin
{
  Q_OBJECT

public:
  RViz();

  ~RViz() override;

  void initPlugin(qt_gui_cpp::PluginContext & context) override;

  void shutdownPlugin() override;

  void saveSettings(
    qt_gui_cpp::Settings & plugin_settings,
    qt_gui_cpp::Settings & instance_settings) const override;

  void restoreSettings(
    const qt_gui_cpp::Settings & plugin_settings,
    const qt_gui_cpp::Settings & instance_settings) override;

  bool hasConfiguration() const override;

  void triggerConfiguration() override;

protected Q_SLOTS:
  void onDisplayConfigChanged();

protected:
  void parseArguments();

  qt_gui_cpp::PluginContext * context_;

  QPointer<RVizVisualizationFrame> widget_;

  std::weak_ptr<rviz_common::ros_integration::RosNodeAbstractionIface> ros_node_;

  bool hide_menu_;
  std::string display_config_;
  bool ogre_log_;

  /** @brief Pointer to menu bar. */
  QMenuBar * menu_bar_;

  enum CmdLineFlags
  {
    OGRE_LOG = 1,
    HIDE_MENU = 2,
    DISPLAY_CONFIG = 4,
  };
  uint8_t set_from_cmdline_ = 0u;
};

}  // namespace rqt_rviz

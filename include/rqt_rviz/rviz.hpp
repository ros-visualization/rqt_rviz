// SPDX-License-Identifier: BSD-3-Clause
// SPDX-FileCopyrightText: Czech Technical University in Prague
// SPDX-FileCopyrightText: Dorian Scholz, TU Darmstadt

#pragma once

#include <QMenuBar>
#include <QPointer>

#include <memory>
#include <string>

#include <tl/expected.hpp>

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

tl::expected<QString, std::string> resolveConfigName(const QString & name, rclcpp::Logger logger);

}  // namespace detail

// This base order releases the node only after VisualizationFrame has been destroyed.
class RVizVisualizationFrame
  : private detail::RVizRosNodeOwner, public rviz_common::VisualizationFrame
{
public:
  explicit RVizVisualizationFrame(
    const std::shared_ptr<rviz_common::ros_integration::RosNodeAbstractionIface> & ros_node);

  //! Outcome of #loadConfigName.
  enum class LoadResult
  {
    //! The configuration was loaded.
    LOADED,
    //! The name could not be resolved to a readable file; nothing was loaded.
    UNRESOLVED,
    //! The user cancelled the prompt about unsaved changes; nothing was loaded.
    CANCELLED,
  };

  /**
   * @brief Initializes RViz with the given configuration name.
   *
   * @param ros_node The ROS node abstraction interface to use for RViz.
   * @param display_config_name The name is resolved to a file system path before RViz is asked
   *                            to load it, so it may be a file system path or a package:// URI.
   *                            If it is empty or cannot be resolved, RViz loads its default
   *                            configuration. Resolution failures are logged and shown in the
   *                            RViz status bar.
   */
  void initialize(
    rviz_common::ros_integration::RosNodeAbstractionIface::WeakPtr ros_node,
    const QString & display_config_name);

  /**
   * @brief Load the configuration with the given name into RViz.
   *
   * The name is resolved by #detail::resolveConfigName and only the resolved file system path
   * is handed over to RViz. An empty name loads RViz's default configuration. Resolution
   * failures are logged and shown in the RViz status bar. If the current configuration has
   * unsaved changes, the user is asked whether to save them first.
   */
  LoadResult loadConfigName(const QString & name);

  /**
   * @brief Whether RViz switched to another configuration file by itself.
   *
   * This happens when the user opens a file or saves to a new file from RViz's own File menu.
   */
  bool displayConfigChangedInRViz() const;

  //! The configuration file RViz currently has open.
  QString displayConfigFile() const;

private:
  void reportConfigError(const QString & name, const std::string & error);

  //! The value of display_config_file_ right after the last load requested by rqt_rviz.
  std::string expected_display_config_file_;
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

  bool eventFilter(QObject * watched, QEvent * event) override;

protected:
  void parseArguments();
  RVizVisualizationFrame::LoadResult setDisplayConfig(const std::string & name);

  //! The configuration name to show and save; follows files opened from RViz's own menu.
  std::string currentDisplayConfig() const;

  qt_gui_cpp::PluginContext * context_;

  QPointer<RVizVisualizationFrame> widget_;

  std::weak_ptr<rviz_common::ros_integration::RosNodeAbstractionIface> ros_node_;

  bool hide_menu_;
  //! Configuration name as entered by the user (a file system path or a package:// URI).
  std::string display_config_;
  bool ogre_log_;

  //! Pointer to menu bar.
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

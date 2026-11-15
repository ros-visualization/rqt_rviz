/*
 * Copyright (c) 2011, Dorian Scholz, TU Darmstadt
 * Copyright (c) 2026, Czech Technical University in Prague
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 *   * Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 *   * Redistributions in binary form must reproduce the above
 *     copyright notice, this list of conditions and the following
 *     disclaimer in the documentation and/or other materials provided
 *     with the distribution.
 *   * Neither the name of the TU Darmstadt nor the names of its
 *     contributors may be used to endorse or promote products derived
 *     from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
 * FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE
 * COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING,
 * BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
 * CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN
 * ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

#pragma once

#if __has_include(<rqt_gui_cpp/plugin.hpp>)
#include <rqt_gui_cpp/plugin.hpp>
#else
#include <rqt_gui_cpp/plugin.h>
#endif
#include <rviz_common/ros_integration/ros_node_abstraction_iface.hpp>
#include <rviz_common/visualization_frame.hpp>

#include <QMenuBar>
#include <QPointer>

#include <cstdint>
#include <map>
#include <memory>
#include <string>
#include <vector>

namespace rqt_rviz {

namespace detail {

class RVizRosNodeAbstraction : public rviz_common::ros_integration::RosNodeAbstractionIface
{
public:
  RVizRosNodeAbstraction(const rclcpp::Node::SharedPtr& node, const std::string& sub_node_name);

  rclcpp::Node::SharedPtr get_raw_node() override;

  std::string get_node_name() const override;

  std::map<std::string, std::vector<std::string>> get_topic_names_and_types() const override;

private:
  rclcpp::Node::SharedPtr supplied_node_;
  rclcpp::Node::SharedPtr node_;
};

struct RVizRosNodeOwner
{
  explicit RVizRosNodeOwner(
    const std::shared_ptr<rviz_common::ros_integration::RosNodeAbstractionIface>& ros_node);

  std::shared_ptr<rviz_common::ros_integration::RosNodeAbstractionIface> ros_node_;
};

}  // namespace detail

// This base order releases the node only after VisualizationFrame has been destroyed.
class RVizVisualizationFrame :
  private detail::RVizRosNodeOwner, public rviz_common::VisualizationFrame
{
public:
  explicit RVizVisualizationFrame(
    const std::shared_ptr<rviz_common::ros_integration::RosNodeAbstractionIface>& ros_node);

  bool prepareForConfiguration();
};

class RViz : public rqt_gui_cpp::Plugin
{
  Q_OBJECT

public:
  RViz();

  ~RViz() override;

  void initPlugin(qt_gui_cpp::PluginContext& context) override;

  void shutdownPlugin() override;

  void saveSettings(
    qt_gui_cpp::Settings& plugin_settings, qt_gui_cpp::Settings& instance_settings) const override;

  void restoreSettings(
    const qt_gui_cpp::Settings& plugin_settings,
    const qt_gui_cpp::Settings& instance_settings) override;

  bool hasConfiguration() const override;

  void triggerConfiguration() override;

protected Q_SLOTS:
  void onDisplayConfigChanged();

protected:
  void parseArguments();

  qt_gui_cpp::PluginContext* context_;

  QPointer<RVizVisualizationFrame> widget_;

  std::weak_ptr<rviz_common::ros_integration::RosNodeAbstractionIface> ros_node_;

  bool hide_menu_;
  std::string display_config_;
  bool ogre_log_;

  /** @brief Pointer to menu bar. */
  QMenuBar* menu_bar_;

  enum CmdLineFlags {
    OGRE_LOG = 1,
    HIDE_MENU = 2,
    DISPLAY_CONFIG = 4,
  };
  uint8_t set_from_cmdline_ = 0u;
};

}

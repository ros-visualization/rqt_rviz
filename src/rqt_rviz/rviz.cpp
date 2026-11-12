/*
 * Copyright (c) 2011, Dorian Scholz, TU Darmstadt
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

#include <atomic>
#include <stdexcept>

#include <boost/program_options.hpp>

#include <pluginlib/class_list_macros.hpp>
#include <rclcpp/rclcpp.hpp>
#if __has_include(<rqt_gui_cpp/plugin.hpp>)
#include <rqt_gui_cpp/plugin.hpp>
#else
#include <rqt_gui_cpp/plugin.h>
#endif
#include <rqt_rviz/config_dialog.h>
#include <rqt_rviz/rviz.h>
#include <rviz_rendering/ogre_logging.hpp>

namespace rqt_rviz {

namespace {

std::atomic<bool> ogre_logging_enabled{false};
std::atomic<unsigned int> rviz_node_instance_id{0};

}  // namespace

RViz::RViz()
  : rqt_gui_cpp::Plugin()
  , context_(0)
  , widget_(0)
  , hide_menu_(false)
  , ogre_log_(false)
{
  setObjectName("RViz");
}

RViz::~RViz()
{
  shutdownPlugin();
}

void RViz::initPlugin(qt_gui_cpp::PluginContext& context)
{
  context_ = &context;

  parseArguments();

  if (!node_)
  {
    RCLCPP_ERROR(rclcpp::get_logger("rqt_rviz"), "No ROS node was supplied by RQt");
    throw std::runtime_error("rqt_rviz was initialized without a ROS node");
  }
  const unsigned int instance_id = rviz_node_instance_id.fetch_add(1) + 1;
  std::string sub_node_name = "rviz";
  if (instance_id > 1)
    sub_node_name += "_" + std::to_string(instance_id);
  auto ros_node = std::make_shared<detail::RVizRosNodeAbstraction>(node_, sub_node_name);
  ros_node_ = ros_node;
  if (ogre_log_ && !ogre_logging_enabled.exchange(true))
    rviz_rendering::OgreLogging::get()->useLogFileAndStandardOut("rqt_rviz_ogre.log");
  widget_ = new RVizVisualizationFrame(ros_node);

  // create own menu bar to disable native menu bars on Unity and Mac
  menu_bar_ = new QMenuBar();
  menu_bar_->setNativeMenuBar(false);
  menu_bar_->setVisible(!hide_menu_);
  widget_->setMenuBar(menu_bar_);
  widget_->setSplashPath(QString());
  widget_->setDisplayTitleFormat("{CONFIG_PATH}/{CONFIG_FILENAME}");
  connect(widget_, &QWidget::windowTitleChanged, this, &RViz::onDisplayConfigChanged);

  widget_->initialize(ros_node, display_config_.c_str());
  onDisplayConfigChanged();

  // disable quit action in menu bar
  QAction* action = menu_bar_->findChild<QAction*>("actQuit");
  if (action)
    action->setVisible(false);

  context.addWidget(widget_);
}

void RViz::shutdownPlugin()
{
  menu_bar_ = nullptr;
  ros_node_.reset();
}

void RViz::onDisplayConfigChanged()
{
  QString config_path = widget_->windowTitle();
  const QString instance_suffix = " (" + QString::number(context_->serialNumber()) + ")";
  if (context_->serialNumber() != 1 && config_path.endsWith(instance_suffix))
    config_path.chop(instance_suffix.size());
  if (config_path.endsWith("[*]"))
    config_path.chop(3);
  if (config_path == "/")
    config_path.clear();
  display_config_ = config_path.toStdString();

  if (context_->serialNumber() != 1 && !widget_->windowTitle().endsWith(instance_suffix))
    widget_->setWindowTitle(widget_->windowTitle() + instance_suffix);
}

void RViz::parseArguments()
{
  namespace po = boost::program_options;

  const QStringList& qargv = context_->argv();

  const int argc = qargv.count();

  // temporary storage for args obtained from qargv - since each QByteArray
  // owns its storage, we need to keep these around until we're done parsing
  // args using boost::program_options
  std::vector<QByteArray> argv_array;
  std::vector<const char *> argv(argc+1);
  argv[0] = ""; // dummy program name

  for (int i = 0; i < argc; ++i)
  {
    argv_array.push_back(qargv.at(i).toLocal8Bit());
    argv[i+1] = argv_array[i].constData();
  }

  po::variables_map vm;
  po::options_description options;
  options.add_options()
    ("display-config,d", po::value<std::string>(), "")
    ("hide-menu,m", "")
    ("ogre-log,l", "");

  try
  {
    po::store(po::parse_command_line(argc+1, argv.data(), options), vm);
    po::notify(vm);

    if (vm.count("hide-menu"))
    {
      hide_menu_ = true;
      set_from_cmdline_ |= HIDE_MENU;
    }

    if (vm.count("display-config"))
    {
      display_config_ = vm["display-config"].as<std::string>();
      set_from_cmdline_ |= DISPLAY_CONFIG;
    }

    if (vm.count("ogre-log"))
    {
      ogre_log_ = true;
      set_from_cmdline_ |= OGRE_LOG;
    }
  }
  catch (std::exception& e)
  {
    RCLCPP_ERROR(rclcpp::get_logger("rqt_rviz"), "Error parsing command line: %s", e.what());
  }
}

void RViz::saveSettings(qt_gui_cpp::Settings& plugin_settings,
                        qt_gui_cpp::Settings& instance_settings) const
{
  instance_settings.setValue("rviz_config_file", display_config_.c_str());
  instance_settings.setValue("hide_menu", hide_menu_);
}

void RViz::restoreSettings(const qt_gui_cpp::Settings& plugin_settings,
                           const qt_gui_cpp::Settings& instance_settings)
{
  if (!(set_from_cmdline_ & DISPLAY_CONFIG) &&
      instance_settings.contains("rviz_config_file"))
  {
    display_config_ = instance_settings.value("rviz_config_file").toString().toStdString();
    widget_->loadDisplayConfig(display_config_.c_str());
  }

  if (!(set_from_cmdline_ & HIDE_MENU) &&
      instance_settings.contains("hide_menu"))
  {
    hide_menu_ = instance_settings.value("hide_menu").toBool();
    menu_bar_->setVisible(!hide_menu_);
  }
}

bool RViz::hasConfiguration() const
{
  return true;
}

void RViz::triggerConfiguration()
{
  if (!widget_->prepareForConfiguration())
    return;

  ConfigDialog dialog(widget_);
  dialog.SetFile(display_config_);
  dialog.SetHide(hide_menu_);

  if (dialog.exec() != QDialog::Accepted)
    return;

  display_config_ = dialog.GetFile();
  hide_menu_ = dialog.GetHide();

  widget_->loadDisplayConfig(display_config_.c_str());
  menu_bar_->setVisible(!hide_menu_);
  set_from_cmdline_ = 0u;
}

}

PLUGINLIB_EXPORT_CLASS(rqt_rviz::RViz, rqt_gui_cpp::Plugin)

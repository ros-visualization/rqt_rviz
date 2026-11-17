// SPDX-License-Identifier: BSD-3-Clause
// SPDX-FileCopyrightText: Czech Technical University in Prague
// SPDX-FileCopyrightText: Dorian Scholz, TU Darmstadt

#include <atomic>
#include <cstdint>
#include <map>
#include <stdexcept>
#include <vector>

#include <boost/program_options.hpp>
#include <tl/expected.hpp>

#include <pluginlib/class_list_macros.hpp>
#include <rclcpp/rclcpp.hpp>
#if __has_include(<rqt_gui_cpp/plugin.hpp>)
#include <rqt_gui_cpp/plugin.hpp>
#else
#include <rqt_gui_cpp/plugin.h>
#endif
#include <rqt_rviz/config_dialog.hpp>
#include <rqt_rviz/rviz.hpp>
#include <rviz_common/logging.hpp>
#include <rviz_rendering/ogre_logging.hpp>

namespace rqt_rviz
{

namespace
{

std::atomic<bool> ogre_logging_enabled{false};
std::atomic<unsigned int> rviz_node_instance_id{0};

}  // namespace

namespace detail
{

class RVizRosNodeAbstraction : public rviz_common::ros_integration::RosNodeAbstractionIface
{
public:
  RVizRosNodeAbstraction(const rclcpp::Node::SharedPtr & node, const std::string & sub_node_name);

  rclcpp::Node::SharedPtr get_raw_node() override;

  std::string get_node_name() const override;

  std::map<std::string, std::vector<std::string>> get_topic_names_and_types() const override;

#ifndef RVIZ_DOESNT_HAVE_SERVICE_SUPPORT
  std::map<std::string, std::vector<std::string>> get_service_names_and_types() const override;
#endif

private:
  rclcpp::Node::SharedPtr supplied_node_;
  rclcpp::Node::SharedPtr node_;
};

}  // namespace detail

detail::RVizRosNodeAbstraction::RVizRosNodeAbstraction(
  const rclcpp::Node::SharedPtr & node, const std::string & sub_node_name)
: supplied_node_(node),
  node_(std::make_shared<rclcpp::Node>(
    std::string(node->get_name()) + "_" + sub_node_name,
    node->get_namespace(),
    rclcpp::NodeOptions().context(node->get_node_base_interface()->get_context())))
{
}

rclcpp::Node::SharedPtr detail::RVizRosNodeAbstraction::get_raw_node()
{
  return node_;
}

std::string detail::RVizRosNodeAbstraction::get_node_name() const
{
  return node_->get_name();
}

std::map<std::string, std::vector<std::string>>
detail::RVizRosNodeAbstraction::get_topic_names_and_types() const
{
  return node_->get_topic_names_and_types();
}

#ifndef RVIZ_DOESNT_HAVE_SERVICE_SUPPORT
std::map<std::string, std::vector<std::string>>
detail::RVizRosNodeAbstraction::get_service_names_and_types() const
{
  return node_->get_service_names_and_types();
}
#endif

detail::RVizRosNodeOwner::RVizRosNodeOwner(
  const std::shared_ptr<rviz_common::ros_integration::RosNodeAbstractionIface> & ros_node)
: ros_node_(ros_node)
{
}

std::shared_ptr<rviz_common::ros_integration::RosNodeAbstractionIface>
detail::createRosNodeAbstraction(
  const rclcpp::Node::SharedPtr & node, const std::string & sub_node_name)
{
  return std::make_shared<RVizRosNodeAbstraction>(node, sub_node_name);
}

tl::expected<QString, std::string> detail::resolveConfigName(
  const QString & name, rclcpp::Logger logger)
{
  const auto resolved_path = resolveDisplayConfigPath(name);
  if (!resolved_path.has_value()) {
    RCLCPP_ERROR(logger, "Failed to resolve configuration path '%s'", name.toStdString().c_str());
    return tl::make_unexpected(resolved_path.error());
  }

  const auto check_result = checkDisplayConfig(*resolved_path);
  if (!check_result.has_value()) {
    RCLCPP_ERROR(logger, "Invalid configuration path '%s'", name.toStdString().c_str());
    return tl::make_unexpected(check_result.error());
  }

  RCLCPP_INFO(logger, "Loading RViz config '%s'", resolved_path->toStdString().c_str());

  return *resolved_path;
}

RVizVisualizationFrame::RVizVisualizationFrame(
  const std::shared_ptr<rviz_common::ros_integration::RosNodeAbstractionIface> & ros_node)
: detail::RVizRosNodeOwner(ros_node), rviz_common::VisualizationFrame(ros_node)
{
}

void RVizVisualizationFrame::initialize(
  rviz_common::ros_integration::RosNodeAbstractionIface::WeakPtr ros_node,
  const QString & display_config_name)
{
  QString resolved_path;

  if (!display_config_name.isEmpty()) {
    const auto node = ros_node.lock();
    if (!node) {
      return;
    }
    const auto maybe_resolved_path = detail::resolveConfigName(
      display_config_name, node->get_raw_node()->get_logger());

    if (maybe_resolved_path.has_value()) {
      resolved_path = *maybe_resolved_path;
    }
  }

  // An empty name leaves RViz on the configuration it loads by itself.
  rviz_common::VisualizationFrame::initialize(ros_node, resolved_path);
}

bool RVizVisualizationFrame::loadConfigName(const QString & name)
{
  if (name.isEmpty()) {
    return true;
  }

  const auto maybe_resolved_path = detail::resolveConfigName(
    name, ros_node_->get_raw_node()->get_logger());
  if (!maybe_resolved_path.has_value()) {
    return false;
  }

  rviz_common::VisualizationFrame::loadDisplayConfig(*maybe_resolved_path);
  return true;
}

bool RVizVisualizationFrame::prepareForConfiguration()
{
  if (!prepareToExit()) {
    return false;
  }
  setWindowModified(false);
  return true;
}

RViz::RViz()
: rqt_gui_cpp::Plugin()
  , context_(nullptr)
  , widget_(nullptr)
  , hide_menu_(false)
  , ogre_log_(false)
{
  setObjectName("RViz");
}

RViz::~RViz()
{
  RViz::shutdownPlugin();
}

void RViz::initPlugin(qt_gui_cpp::PluginContext & context)
{
  context_ = &context;

  if (!node_) {
    RCLCPP_ERROR(rclcpp::get_logger("rqt_rviz"), "No ROS node was supplied by RQt");
    throw std::runtime_error("rqt_rviz was initialized without a ROS node");
  }
  const unsigned int instance_id = rviz_node_instance_id.fetch_add(1) + 1;
  std::string sub_node_name = "rviz";
  if (instance_id > 1) {
    sub_node_name += "_" + std::to_string(instance_id);
  }
  const auto ros_node = std::make_shared<detail::RVizRosNodeAbstraction>(node_, sub_node_name);
  ros_node_ = ros_node;

  parseArguments();

  auto logger = ros_node->get_raw_node()->get_logger();
  // install logging handlers to route logging through ROS's logging system
  rviz_common::set_logging_handlers(
    [logger](const std::string & msg, const std::string &, size_t) {
      RCLCPP_DEBUG(logger, "%s", msg.c_str());
    },
    [logger](const std::string & msg, const std::string &, size_t) {
      RCLCPP_INFO(logger, "%s", msg.c_str());
    },
    [logger](const std::string & msg, const std::string &, size_t) {
      RCLCPP_WARN(logger, "%s", msg.c_str());
    },
    [logger](const std::string & msg, const std::string &, size_t) {
      RCLCPP_ERROR(logger, "%s", msg.c_str());
    }
  );

  rviz_common::install_rviz_rendering_log_handlers();

  if (ogre_log_ && !ogre_logging_enabled.exchange(true)) {
    rviz_rendering::OgreLogging::get()->useLogFileAndStandardOut("rqt_rviz_ogre.log");
  }
  widget_ = new RVizVisualizationFrame(ros_node);

  // create own menu bar to disable native menu bars on Unity and Mac
  menu_bar_ = new QMenuBar();
  menu_bar_->setNativeMenuBar(false);
  menu_bar_->setVisible(!hide_menu_);
  widget_->setMenuBar(menu_bar_);
  widget_->setSplashPath(QString());

  // Overwrite the default RViz[*] title
  widget_->setWindowTitle("RViz");
  if (context.serialNumber() > 1) {
    widget_->setWindowTitle("RViz (" + QString::number(context.serialNumber()) + ")");
  }

  const auto title = widget_->windowTitle();

  widget_->setDisplayTitleFormat(title);
  widget_->initialize(ros_node, display_config_.c_str());
  widget_->setWindowTitle(title);

  // disable quit action in menu bar
  if (const auto action = menu_bar_->findChild<QAction *>("actQuit")) {
    action->setVisible(false);
  }

  context.addWidget(widget_);
}

void RViz::shutdownPlugin()
{
  menu_bar_ = nullptr;
  ros_node_.reset();
}

bool RViz::setDisplayConfig(const std::string & name)
{
  // Remember the name exactly as it was entered.
  display_config_ = name;

  // The RViz widget normally sets its window title after loading the new config,
  // but we want the title of the widget to stay as it was. Setting DisplayTitleFormat
  // is a workaround - the widget still changes the title, but at least it changes it
  // to exactly what we need.
  const auto title = widget_->windowTitle();
  widget_->setDisplayTitleFormat(title);

  if (!widget_->loadConfigName(QString(name.c_str()))) {
    return false;
  }

  widget_->setWindowTitle(title);
  return true;
}

void RViz::parseArguments()
{
  namespace po = boost::program_options;
  const auto & qargv = context_->argv();
  const auto argc = qargv.count();

  // temporary storage for args obtained from qargv - since each QByteArray
  // owns its storage, we need to keep these around until we're done parsing
  // args using boost::program_options
  std::vector<QByteArray> argv_array;
  std::vector<const char *> argv(argc + 1);
  argv[0] = "";  // dummy program name

  for (int i = 0; i < argc; ++i) {
    argv_array.push_back(qargv.at(i).toLocal8Bit());
    argv[i + 1] = argv_array[i].constData();
  }

  po::variables_map vm;
  po::options_description options;
  options.add_options()
  ("display-config,d", po::value<std::string>(), "")
  ("hide-menu,m", "")
  ("ogre-log,l", "");

  try {
    po::store(po::parse_command_line(argc + 1, argv.data(), options), vm);
    po::notify(vm);

    if (vm.count("hide-menu")) {
      hide_menu_ = true;
      set_from_cmdline_ |= HIDE_MENU;
    }

    if (vm.count("display-config")) {
      display_config_ = vm["display-config"].as<std::string>();
      set_from_cmdline_ |= DISPLAY_CONFIG;
    }

    if (vm.count("ogre-log")) {
      ogre_log_ = true;
      set_from_cmdline_ |= OGRE_LOG;
    }
  } catch (const std::exception & e) {
    const auto node = ros_node_.lock();
    if (node) {
      RCLCPP_ERROR(node->get_raw_node()->get_logger(), "Error parsing command line: %s", e.what());
    } else {
      RCLCPP_ERROR(rclcpp::get_logger("rqt_rviz"), "Error parsing command line: %s", e.what());
    }
  }
}

void RViz::saveSettings(
  qt_gui_cpp::Settings & plugin_settings, qt_gui_cpp::Settings & instance_settings) const
{
  instance_settings.setValue("display_config", display_config_.c_str());
  instance_settings.setValue("hide_menu", hide_menu_);
}

void RViz::restoreSettings(
  const qt_gui_cpp::Settings & plugin_settings, const qt_gui_cpp::Settings & instance_settings)
{
  // CLI args should win over whatever is stored in the perspective

  QString title;
  if (widget_) {
    title = widget_->windowTitle();
    widget_->setDisplayTitleFormat(title);
  }

  if (!(set_from_cmdline_ & DISPLAY_CONFIG) && instance_settings.contains("display_config")) {
    setDisplayConfig(instance_settings.value("display_config").toString().toStdString());
  }

  if (widget_) {
    widget_->setWindowTitle(title);
  }

  if (!(set_from_cmdline_ & HIDE_MENU) && instance_settings.contains("hide_menu")) {
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
  if (!widget_->prepareForConfiguration()) {
    return;
  }

  ConfigDialog dialog(widget_);
  dialog.SetFile(display_config_);
  dialog.SetHide(hide_menu_);

  if (dialog.exec() != QDialog::Accepted) {
    return;
  }

  display_config_ = dialog.GetFile();
  hide_menu_ = dialog.GetHide();

  setDisplayConfig(display_config_);
  menu_bar_->setVisible(!hide_menu_);
  set_from_cmdline_ = 0u;
}

}  // namespace rqt_rviz

PLUGINLIB_EXPORT_CLASS(rqt_rviz::RViz, rqt_gui_cpp::Plugin)

// SPDX-License-Identifier: BSD-3-Clause
// SPDX-FileCopyrightText: Czech Technical University in Prague

#include <gtest/gtest.h>

#include <QApplication>
#include <QMenuBar>

#include <memory>
#include <string>

#include <rqt_rviz/config_dialog.hpp>
#include <rqt_rviz/rviz.hpp>

#include "mock_handler.hpp"

namespace
{

class TestRViz : public rqt_rviz::RViz
{
public:
  void parse(qt_gui_cpp::PluginContext & context)
  {
    context_ = &context;
    parseArguments();
  }

  void setMenuBar(QMenuBar * menu_bar)
  {
    menu_bar_ = menu_bar;
  }

  bool menuBarHidden() const
  {
    return menu_bar_->isHidden();
  }

  QString config() const
  {
    return QString::fromStdString(display_config_);
  }

  bool menuHidden() const
  {
    return hide_menu_;
  }
};

TEST(RVizTest, CommandLineOverridesRestoredSettings)
{
  MockHandler handler;
  qt_gui_cpp::PluginContext context(
    &handler, 1, {"--display-config=command_line.rviz", "--hide-menu"});
  TestRViz plugin;
  plugin.parse(context);

  EXPECT_EQ(plugin.config(), "command_line.rviz");
  EXPECT_TRUE(plugin.menuHidden());

  MockSettings global_backend;
  MockSettings restored_backend;
  restored_backend.values["rviz_config_file"] = "restored.rviz";
  restored_backend.values["hide_menu"] = false;
  qt_gui_cpp::Settings global(&global_backend);
  qt_gui_cpp::Settings restored(&restored_backend);
  plugin.restoreSettings(global, restored);

  EXPECT_EQ(plugin.config(), "command_line.rviz");
  EXPECT_TRUE(plugin.menuHidden());

  MockSettings saved_backend;
  qt_gui_cpp::Settings saved(&saved_backend);
  plugin.saveSettings(global, saved);
  EXPECT_EQ(saved_backend.value("rviz_config_file").toString(), "command_line.rviz");
  EXPECT_TRUE(saved_backend.value("hide_menu").toBool());
}

TEST(RVizTest, RestoresMenuVisibilityWithoutOverridingCommandLineConfig)
{
  MockHandler handler;
  qt_gui_cpp::PluginContext source_context(
    &handler, 1, {"--display-config=source.rviz", "--hide-menu"});
  TestRViz source;
  source.parse(source_context);

  MockSettings global_backend;
  MockSettings saved_backend;
  qt_gui_cpp::Settings global(&global_backend);
  qt_gui_cpp::Settings saved(&saved_backend);
  source.saveSettings(global, saved);

  QMenuBar menu_bar;
  qt_gui_cpp::PluginContext destination_context(
    &handler, 2, {"--display-config=destination.rviz"});
  TestRViz destination;
  destination.parse(destination_context);
  destination.setMenuBar(&menu_bar);
  destination.restoreSettings(global, saved);

  EXPECT_EQ(destination.config(), "destination.rviz");
  EXPECT_TRUE(destination.menuHidden());
  EXPECT_TRUE(destination.menuBarHidden());
}

TEST(RVizTest, ConfigurationDialogRoundTrip)
{
  rqt_rviz::ConfigDialog dialog;
  dialog.SetFile("custom.rviz");
  dialog.SetHide(true);

  EXPECT_EQ(dialog.GetFile(), "custom.rviz");
  EXPECT_TRUE(dialog.GetHide());

  dialog.SetFile("");
  dialog.SetHide(false);
  EXPECT_TRUE(dialog.GetFile().empty());
  EXPECT_FALSE(dialog.GetHide());
}

TEST(RVizTest, NodeAbstractionsAreIndependent)
{
  auto supplied_node = std::make_shared<rclcpp::Node>("rqt_provider", "/test");
  auto first = std::make_shared<rqt_rviz::detail::RVizRosNodeAbstraction>(
    supplied_node, "rviz");
  auto second = std::make_shared<rqt_rviz::detail::RVizRosNodeAbstraction>(
    supplied_node, "rviz_2");

  ASSERT_NE(first->get_raw_node(), nullptr);
  ASSERT_NE(second->get_raw_node(), nullptr);
  EXPECT_NE(first->get_raw_node(), second->get_raw_node());
  EXPECT_NE(first->get_raw_node().get(), supplied_node.get());
  EXPECT_EQ(first->get_node_name(), "rqt_provider_rviz");
  EXPECT_EQ(second->get_node_name(), "rqt_provider_rviz_2");
  EXPECT_STREQ(first->get_raw_node()->get_fully_qualified_name(), "/test/rqt_provider_rviz");
  EXPECT_STREQ(second->get_raw_node()->get_fully_qualified_name(), "/test/rqt_provider_rviz_2");
  EXPECT_EQ(
    first->get_raw_node()->get_node_base_interface()->get_context(),
    supplied_node->get_node_base_interface()->get_context());

  std::weak_ptr<rviz_common::ros_integration::RosNodeAbstractionIface> first_weak = first;
  first.reset();
  EXPECT_TRUE(first_weak.expired());
  ASSERT_NE(second->get_raw_node(), nullptr);
  EXPECT_EQ(second->get_node_name(), "rqt_provider_rviz_2");
}

}  // namespace

int main(int argc, char ** argv)
{
  testing::InitGoogleTest(&argc, argv);
  rclcpp::init(argc, argv);
  QApplication application(argc, argv);
  const int result = RUN_ALL_TESTS();
  rclcpp::shutdown();
  return result;
}

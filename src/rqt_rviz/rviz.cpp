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

#include <OGRE/OgreLogManager.h>

#include <QCloseEvent>
#include <QMenuBar>
#include <QFileDialog>

#include <pluginlib/class_list_macros.h>
#include <boost/program_options.hpp>
#include <fstream>

#include <rqt_rviz/rviz.h>


namespace rqt_rviz {

RViz::RViz()
  : rqt_gui_cpp::Plugin()
  , context_(0)
  , widget_(0)
  , log_(0)
  , hide_menu_(false)
  , ogre_log_(false)
{
  setObjectName("RViz");
}

RViz::~RViz()
{
  Ogre::LogManager* log_manager = Ogre::LogManager::getSingletonPtr();
  if (log_manager && log_)
  {
    log_manager->destroyLog(log_);
  }
}

void RViz::initPlugin(qt_gui_cpp::PluginContext& context)
{
  context_ = &context;

  parseArguments();

  // prevent output of Ogre stuff to console
  Ogre::LogManager* log_manager = Ogre::LogManager::getSingletonPtr();
  if (!log_manager)
  {
    log_manager = new Ogre::LogManager();
  }
  QString filename = QString("rqt_rviz_ogre") + (context.serialNumber() > 1 ? QString::number(context.serialNumber()) : QString("")) + QString(".log");
  log_ = log_manager->createLog(filename.toStdString().c_str(), false, false, !ogre_log_);

  widget_ = new rviz::VisualizationFrame();

  // create own menu bar to disable native menu bars on Unity and Mac
  QMenuBar* menu_bar = new QMenuBar();
  menu_bar->setNativeMenuBar(false);
  menu_bar->setVisible(!hide_menu_);
  widget_->setMenuBar(menu_bar);

  widget_->initialize(display_config_.c_str());

  // disable quit action in menu bar
  QMenu* menu = 0;
  {
    // find first menu in menu bar
    const QObjectList& children = menu_bar->children();
    for (QObjectList::const_iterator it = children.begin(); !menu && it != children.end(); it++)
    {
      menu = dynamic_cast<QMenu*>(*it);
    }
  }
  if (menu)
  {
    // hide last action in menu
    const QObjectList& children = menu->children();
    if (!children.empty())
    {
      QAction* action = dynamic_cast<QAction*>(children.last());
      if (action)
      {
        action->setVisible(false);
      }
    }
  }

  widget_->setWindowTitle("RViz[*]");
  if (context.serialNumber() != 1)
  {
    widget_->setWindowTitle(widget_->windowTitle() + " (" + QString::number(context.serialNumber()) + ")");
  }
  context.addWidget(widget_);

  // trigger deleteLater for plugin when widget or frame is closed
  widget_->installEventFilter(this);
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
  const char *argv[argc+1];
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
    po::store(po::parse_command_line(argc+1, argv, options), vm);
    po::notify(vm);

    if (vm.count("hide-menu"))
    {
      ROS_INFO_STREAM("HIDE MENU TRUE");
      hide_menu_ = true;
    }
    else
      ROS_INFO_STREAM("HIDE MENU FALSE");

    if (vm.count("display-config"))
    {
      display_config_ = vm["display-config"].as<std::string>();
    }

    if (vm.count("ogre-log"))
    {
      ogre_log_ = true;
    }
  }
  catch (std::exception& e)
  {
    ROS_ERROR("Error parsing command line: %s", e.what());
  }
}

void RViz::saveSettings(qt_gui_cpp::Settings& plugin_settings, qt_gui_cpp::Settings& instance_settings) const {
  instance_settings.setValue("rviz_config_file", display_config_.c_str());
  instance_settings.setValue("hide_menu", hide_menu_);
}

void RViz::restoreSettings(const qt_gui_cpp::Settings& plugin_settings, const qt_gui_cpp::Settings& instance_settings) {
  if(instance_settings.contains("rviz_config_file")) {
    display_config_ = instance_settings.value("rviz_config_file").toString().toLocal8Bit().constData();;
    // Read config from file
    std::ifstream cfgFile;
    cfgFile.open(display_config_.c_str(), std::ifstream::in);
    if (cfgFile.good()){
      std::stringstream strStream;
      strStream << cfgFile.rdbuf();
      display_config_ = strStream.str();
      // Set it
      // No idea how to do this properly
    }
    else
      ROS_ERROR_STREAM("Non existing config file: " << display_config_);
  }

  if(instance_settings.contains("hide_menu")) {
    bool hide_menu_saved_ = instance_settings.value("hide_menu").toBool();
    ROS_INFO_STREAM("We would set hide_menu to: " << hide_menu_);
  //   QMenuBar* menu_bar = new QMenuBar();
  //   menu_bar->setNativeMenuBar(false);
  //   // To deal with the commandline arguments, they take precedence
  //   if (hide_menu_saved_ && !hide_menu_){
  //       hide_menu_ = hide_menu_saved_;
  //     }
  //   menu_bar->setVisible(!hide_menu_);
  //   widget_->setMenuBar(menu_bar);
    }

}

bool RViz::hasConfiguration() const{
  return true;
}

void RViz::triggerConfiguration(){
  // Create a dialog with the config file and hide menu
  // It would be nice to be able to specify a path in a ROS package
  // way, e.g.: $(find mypkg)/config/cfg.rviz
  // or a command to be able to do `rospack find mypkg`/config/cfg.rviz
  // so people could ship a launchfile with a perspective
  // that can be used in other machines
  // (otherwise the config would be like /home/user/cfg.rviz)

  // I have no clue how to implement this in C++ and I haven't found a single example
  ROS_INFO_STREAM("Clicked configuration!");
  // Ideally we want to show a custom dialog with the checkbox for the hide menu...
  // this is all I could do
  QString filename = QFileDialog::getOpenFileName(0,
    tr("Choose config file:"), "", tr("Rviz config file (*.rviz)"));
  ROS_INFO_STREAM("Chosen: " << filename.toLocal8Bit().constData());
  // if the user actually chose a file
  if (filename.size() > 0){
    std::ifstream cfgFile;
    cfgFile.open(filename.toLocal8Bit().constData(), std::ifstream::in);
    // Check if the file exists
    if (cfgFile.good()){
      // Set it
      display_config_ = filename.toLocal8Bit().constData();
      // No idea how to do this properly
    }
  }

}


bool RViz::eventFilter(QObject* watched, QEvent* event)
{
  if (watched == widget_ && event->type() == QEvent::Close)
  {
    event->ignore();
    context_->closePlugin();
    return true;
  }

  return QObject::eventFilter(watched, event);
}

}

PLUGINLIB_EXPORT_CLASS(rqt_rviz::RViz, rqt_gui_cpp::Plugin)

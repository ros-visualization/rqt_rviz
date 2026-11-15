<!--
SPDX-License-Identifier: BSD-3-Clause
SPDX-FileCopyrightText: Czech Technical University in Prague
-->

# rqt\_rviz

rqt\_rviz is a ROS package that can embed RViz in RQt windows.

It is able to embed multiple RViz instances in a single RQt window.

![rqt_rviz](doc/plugin.png)

![rqt_rviz settings](doc/settings.png)

## Usage

Either insert the plugin into a running RQt GUI from menu Plugin -> Visualization -> rqt\_rviz,
or call the standalone launcher from command line:

    ros2 run rqt_rviz rqt_rviz

## Settings

The plugin has the following settings:

| Setting          | Description                         |
|------------------|-------------------------------------|
| rviz_config_file | Path to the RViz configuration file |
| hide_menu        | Hide the RViz menu bar              |

The path to the RViz config file takes either relative paths, absolute paths, or URIs of
type `package://ros_package_name/path/to/file.rviz`. If the referenced package is not found,
the URI is treated as an absolute path (which will also most probably fail).

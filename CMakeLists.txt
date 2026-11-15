cmake_minimum_required(VERSION 3.20.2)
project(rqt_rviz)

if(NOT CMAKE_CXX_STANDARD)
  set(CMAKE_CXX_STANDARD 17)
endif()
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)

find_package(ament_cmake REQUIRED)
find_package(pluginlib REQUIRED)
find_package(qt_gui_cpp REQUIRED)
find_package(rclcpp REQUIRED)
find_package(rqt_gui_cpp REQUIRED)
find_package(rviz_common REQUIRED)
find_package(rviz_rendering REQUIRED)

find_package(Boost REQUIRED COMPONENTS program_options)
# Qt is transitively required from rviz_common
# We intentionally do not pull Qt here to simplify integrating with the Qt5/6/7?
# variants of RQt and RViz.

set(rqt_rviz_SRCS
  src/rqt_rviz/config_dialog.cpp
  src/rqt_rviz/rviz.cpp
)

set(rqt_rviz_HDRS
  include/rqt_rviz/config_dialog.hpp
  include/rqt_rviz/rviz.hpp
)

add_library(${PROJECT_NAME} SHARED ${rqt_rviz_SRCS} ${rqt_rviz_HDRS})
target_include_directories(${PROJECT_NAME}
  PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:include/${PROJECT_NAME}>
)
target_link_libraries(${PROJECT_NAME}
  PUBLIC
    rqt_gui_cpp::rqt_gui_cpp
    rviz_common::rviz_common
    # Qt is transitively required from rviz_common
  PRIVATE
    pluginlib::pluginlib
    qt_gui_cpp::qt_gui_cpp
    rclcpp::rclcpp
    rviz_rendering::rviz_rendering
    Boost::program_options
)

install(TARGETS ${PROJECT_NAME}
  EXPORT export_${PROJECT_NAME}
  ARCHIVE DESTINATION lib
  LIBRARY DESTINATION lib
  RUNTIME DESTINATION bin
  INCLUDES DESTINATION include
)

install(DIRECTORY include/
  DESTINATION include/${PROJECT_NAME}
)

install(PROGRAMS scripts/rqt_rviz
  DESTINATION lib/${PROJECT_NAME}
)

install(FILES plugin.xml
  DESTINATION share/${PROJECT_NAME}
)

pluginlib_export_plugin_description_file(rqt_gui plugin.xml)

ament_export_include_directories(include)
ament_export_libraries(${PROJECT_NAME})
ament_export_targets(export_${PROJECT_NAME} HAS_LIBRARY_TARGET)
ament_export_dependencies(
  # Qt is transitively exported by rviz_common
  rqt_gui_cpp
  rviz_common
)

ament_package()

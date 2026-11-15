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

if(BUILD_TESTING)
  find_package(ament_cmake_gtest REQUIRED)
  find_package(ament_cmake_test REQUIRED)
  find_package(Python3 REQUIRED COMPONENTS Interpreter)
  # Qt is transitively found by the main plugin target
  find_program(XVFB_RUN_EXECUTABLE xvfb-run REQUIRED)

  ament_add_gtest(test_rviz
    test/offscreen_test.cpp
    test/mock_handler.hpp
  )
  target_link_libraries(test_rviz
    ${PROJECT_NAME}
    qt_gui_cpp::qt_gui_cpp
    rclcpp::rclcpp
    # Qt is included transitively
  )
  set_tests_properties(test_rviz PROPERTIES
    ENVIRONMENT "QT_QPA_PLATFORM=offscreen;LIBGL_ALWAYS_SOFTWARE=1"
    TIMEOUT 120
  )

  ament_add_test(test_rqt_window
    COMMAND
      "${XVFB_RUN_EXECUTABLE}" -a -s "-screen 0 1280x1024x24"
      "${Python3_EXECUTABLE}" -u "${CMAKE_CURRENT_SOURCE_DIR}/test/rqt_window_test.py"
    ENV
      "QT_QPA_PLATFORM=xcb"
      "LIBGL_ALWAYS_SOFTWARE=1"
      "PYTHONUNBUFFERED=1"
      "ROS_AUTOMATIC_DISCOVERY_RANGE=LOCALHOST"
    GENERATE_RESULT_FOR_RETURN_CODE_ZERO
    TIMEOUT 120
  )
endif()

ament_package()

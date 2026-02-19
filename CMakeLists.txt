cmake_minimum_required(VERSION 3.8)
project(rviz2_reconfigure)

if(CMAKE_COMPILER_IS_GNUCXX OR CMAKE_CXX_COMPILER_ID MATCHES "Clang")
  add_compile_options(-Wall -Wextra -Wpedantic)
endif()

# find dependencies
find_package(ament_cmake REQUIRED)
find_package(rclcpp REQUIRED)
find_package(pluginlib REQUIRED)
find_package(rviz_common REQUIRED)
find_package(Qt5 ${rviz_QT_VERSION} REQUIRED Core Widgets)
macro(qt_wrap_ui)
  qt5_wrap_ui(${ARGN})
endmacro()

add_definitions(-DQT_NO_KEYWORDS)
set(CMAKE_INCLUDE_CURRENT_DIR ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)

pluginlib_export_plugin_description_file(rviz_common plugins_description.xml)

qt5_wrap_ui(UIC_FILES
  src/ui/reconfigure.ui
  src/ui/param_dialog.ui
)

add_library(rviz2_reconfigure SHARED
  ${UIC_FILES}
  include/rviz2_reconfigure/reconfigure.hpp
  src/reconfigure.cpp
)

target_compile_features(rviz2_reconfigure PUBLIC c_std_99 cxx_std_17)
target_include_directories(rviz2_reconfigure PUBLIC
  $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
  $<INSTALL_INTERFACE:include>
  ${Qt5Core_INCLUDE_DIRS}
  ${Qt5Widgets_INCLUDE_DIRS}
)
ament_target_dependencies(rviz2_reconfigure
  "rclcpp"
  "pluginlib"
  "rviz_common"
  "Qt5"
)

# target_compile_definitions(rviz2_reconfigure PUBLIC "RVIZ2_RECONFIGURE_BUILDING_LIBRARY")

install(
  DIRECTORY include/
  DESTINATION include/${PROJECT_NAME}
)
# install(
#   DIRECTORY icons
#   DESTINATION share/${PROJECT_NAME}
# )
install(
  TARGETS rviz2_reconfigure
  EXPORT export_${PROJECT_NAME}
  ARCHIVE DESTINATION lib
  LIBRARY DESTINATION lib
  RUNTIME DESTINATION bin
)

# install(DIRECTORY
#   test
#   DESTINATION share/${PROJECT_NAME}
# )

if(BUILD_TESTING)
  find_package(ament_lint_auto REQUIRED)
  # the following line skips the linter which checks for copyrights
  # comment the line when a copyright and license is added to all source files
  set(ament_cmake_copyright_FOUND TRUE)
  # the following line skips cpplint (only works in a git repo)
  # comment the line when this package is in a git repo and when
  # a copyright and license is added to all source files
  set(ament_cmake_cpplint_FOUND TRUE)
  ament_lint_auto_find_test_dependencies()
endif()

ament_export_include_directories(
  include
)
ament_export_libraries(
  rviz2_reconfigure
)
ament_export_targets(
  export_${PROJECT_NAME}
)

ament_package()

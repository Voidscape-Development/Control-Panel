cmake_minimum_required(VERSION 3.28...3.30)

# Standalone example of a plugin that extends Control Panel. Copy
# include/control-panel-api.h into your own plugin; that's the only file needed.
#
#   cmake -S examples/example-extension -B build_example
#   cmake --build build_example

project(control-panel-example-extension LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(libobs REQUIRED)
find_package(Qt6 REQUIRED COMPONENTS Widgets)

add_library(control-panel-example-extension MODULE example-extension.cpp)
target_include_directories(control-panel-example-extension PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/../../include")
target_link_libraries(control-panel-example-extension PRIVATE OBS::libobs Qt6::Widgets)
set_target_properties(control-panel-example-extension PROPERTIES PREFIX "")

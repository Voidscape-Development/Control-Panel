cmake_minimum_required(VERSION 3.28...3.30)

include("${CMAKE_CURRENT_SOURCE_DIR}/cmake/common/bootstrap.cmake" NO_POLICY_SCOPE)

project(${_name} VERSION ${_version})

# Control Panel is a docked Qt UI: both are required.
option(ENABLE_FRONTEND_API "Use obs-frontend-api for UI functionality" ON)
option(ENABLE_QT "Use Qt functionality" ON)

if(NOT ENABLE_FRONTEND_API OR NOT ENABLE_QT)
  message(FATAL_ERROR "Control Panel requires ENABLE_FRONTEND_API and ENABLE_QT")
endif()

include(compilerconfig)
include(defaults)
include(helpers)

add_library(${CMAKE_PROJECT_NAME} MODULE)

find_package(libobs REQUIRED)
target_link_libraries(${CMAKE_PROJECT_NAME} PRIVATE OBS::libobs)

if(ENABLE_FRONTEND_API)
  find_package(obs-frontend-api REQUIRED)
  target_link_libraries(${CMAKE_PROJECT_NAME} PRIVATE OBS::obs-frontend-api)
endif()

if(ENABLE_QT)
  find_package(Qt6 REQUIRED COMPONENTS Widgets Core Svg)
  target_link_libraries(${CMAKE_PROJECT_NAME} PRIVATE Qt6::Core Qt6::Widgets Qt6::Svg)
  target_compile_options(
    ${CMAKE_PROJECT_NAME}
    PRIVATE $<$<C_COMPILER_ID:Clang,AppleClang>:-Wno-quoted-include-in-framework-header -Wno-comma>
  )
  set_target_properties(
    ${CMAKE_PROJECT_NAME}
    PROPERTIES AUTOMOC ON AUTOUIC ON AUTORCC ON
  )
endif()

target_include_directories(
  ${CMAKE_PROJECT_NAME}
  PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/include" "${CMAKE_CURRENT_SOURCE_DIR}/deps/obs-websocket-api"
)

target_sources(
  ${CMAKE_PROJECT_NAME}
  PRIVATE
    include/control-panel-api.h
    src/plugin-main.cpp
    src/model/grid.cpp
    src/model/grid.hpp
    src/model/panel-model.cpp
    src/model/panel-model.hpp
    src/model/panel-store.cpp
    src/model/panel-store.hpp
    src/core/builtin-actions.cpp
    src/core/builtin-widgets.cpp
    src/core/obs-util.cpp
    src/core/obs-util.hpp
    src/core/panel-hotkeys.cpp
    src/core/panel-hotkeys.hpp
    src/core/runtime.cpp
    src/core/runtime.hpp
    src/core/type-registry.cpp
    src/core/type-registry.hpp
    src/core/websocket-vendor.cpp
    src/core/websocket-vendor.hpp
    src/ui/cell-inspector.cpp
    src/ui/cell-inspector.hpp
    src/ui/cell-painter.cpp
    src/ui/cell-painter.hpp
    src/ui/dock-manager.cpp
    src/ui/dock-manager.hpp
    src/ui/edit-ops.cpp
    src/ui/edit-ops.hpp
    src/ui/editor-dialog.cpp
    src/ui/editor-dialog.hpp
    src/ui/grid-canvas.cpp
    src/ui/grid-canvas.hpp
    src/ui/icons.cpp
    src/ui/icons.hpp
    src/ui/panel-view.cpp
    src/ui/panel-view.hpp
    src/ui/properties-form.cpp
    src/ui/properties-form.hpp
    src/ui/resources.qrc
)

set_target_properties_plugin(${CMAKE_PROJECT_NAME} PROPERTIES OUTPUT_NAME ${_name})

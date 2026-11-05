cmake_minimum_required(VERSION 3.28...3.30)

# Unit tests for the OBS-independent model layer. Built standalone so they only
# need Qt:
#
#   cmake -S tests -B build_tests -G Ninja
#   cmake --build build_tests
#   ctest --test-dir build_tests --output-on-failure

project(control-panel-tests LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)

find_package(Qt6 REQUIRED COMPONENTS Core Gui Svg Test)

enable_testing()

set(_model_dir "${CMAKE_CURRENT_SOURCE_DIR}/../src/model")
add_library(
  control-panel-model
  STATIC
  "${_model_dir}/panel-model.cpp"
  "${_model_dir}/panel-model.hpp"
  "${_model_dir}/grid.cpp"
  "${_model_dir}/grid.hpp"
  "${_model_dir}/panel-store.cpp"
  "${_model_dir}/panel-store.hpp"
  "${_model_dir}/icon-ref.cpp"
  "${_model_dir}/icon-ref.hpp"
  "${_model_dir}/material-symbols.cpp"
  "${_model_dir}/material-symbols.hpp"
)
target_include_directories(control-panel-model PUBLIC "${_model_dir}")
target_link_libraries(control-panel-model PUBLIC Qt6::Core Qt6::Gui)

foreach(_test IN ITEMS test-model test-grid test-store test-icons)
  add_executable(${_test} ${_test}.cpp)
  target_link_libraries(${_test} PRIVATE control-panel-model Qt6::Svg Qt6::Test)
  target_compile_definitions(${_test} PRIVATE CP_SOURCE_DIR="${CMAKE_CURRENT_SOURCE_DIR}/..")
  add_test(NAME ${_test} COMMAND ${_test})
  set_tests_properties(${_test} PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen")
endforeach()

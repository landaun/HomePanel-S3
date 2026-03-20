# For more information about build system see
# https://docs.espressif.com/projects/esp-idf/en/latest/api-guides/build-system.html
# The following five lines of boilerplate have to be in your project's
# CMakeLists in this exact order for cmake to work correctly
cmake_minimum_required(VERSION 3.5)

include($ENV{IDF_PATH}/tools/cmake/project.cmake)

# Use custom LVGL configuration
set(LV_CONF_PATH "${CMAKE_SOURCE_DIR}/lv_conf.h")

# Restrict component search to project components only
set(EXTRA_COMPONENT_DIRS "${CMAKE_SOURCE_DIR}/components")

add_compile_options("-Wno-attributes")
project(lvgl_porting)

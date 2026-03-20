cmake_minimum_required(VERSION 3.21)

project(panel_simulator C)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)

set(ROOT_DIR "${CMAKE_SOURCE_DIR}/..")
set(LV_CONF_PATH "${ROOT_DIR}/lv_conf.h" CACHE STRING "LVGL configuration file" FORCE)

# LVGL static library
add_subdirectory("${ROOT_DIR}/components/lvgl__lvgl" "${CMAKE_BINARY_DIR}/lvgl")

set(SIMULATOR_SOURCES
    src/main.c
    src/win32_port.c
    src/sim_data.c
    stubs/home_assistant_stubs.c
    stubs/lvgl_port_stub.c
    stubs/simulator_log.c
    "${ROOT_DIR}/components/cjson/cJSON.c"
    "${ROOT_DIR}/main/src/tabbed_ui.c"
    "${ROOT_DIR}/main/src/ui_scene_grid.c"
    "${ROOT_DIR}/main/ui_icons.c"
    "${ROOT_DIR}/main/ui_icons_font.c"
    "${ROOT_DIR}/main/ui_style.c"
)

add_executable(panel_simulator ${SIMULATOR_SOURCES})

target_include_directories(
    panel_simulator
    PRIVATE
        "${CMAKE_SOURCE_DIR}/include"
        "${ROOT_DIR}/components/cjson"
        "${ROOT_DIR}/main/include"
        "${ROOT_DIR}/main"
)

target_compile_definitions(
    panel_simulator
    PRIVATE
        PANEL_SIMULATOR
        LV_LVGL_H_INCLUDE_SIMPLE
        LV_CONF_INCLUDE_SIMPLE
        _CRT_SECURE_NO_WARNINGS
        WIN32_LEAN_AND_MEAN
)

target_link_libraries(
    panel_simulator
    PRIVATE
        lvgl::lvgl
        user32
        gdi32
        winmm
)

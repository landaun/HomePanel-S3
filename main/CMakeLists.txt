# WiFi credentials: environment variables take priority over sdkconfig
set(WIFI_SSID_VAL "")
if(DEFINED ENV{WIFI_SSID} AND NOT "$ENV{WIFI_SSID}" STREQUAL "")
    set(WIFI_SSID_VAL "$ENV{WIFI_SSID}")
elseif(DEFINED CONFIG_WIFI_SSID AND NOT "${CONFIG_WIFI_SSID}" STREQUAL "")
    set(WIFI_SSID_VAL "${CONFIG_WIFI_SSID}")
endif()

set(WIFI_PASSWORD_VAL "")
if(DEFINED ENV{WIFI_PASSWORD} AND NOT "$ENV{WIFI_PASSWORD}" STREQUAL "")
    set(WIFI_PASSWORD_VAL "$ENV{WIFI_PASSWORD}")
elseif(DEFINED CONFIG_WIFI_PASSWORD AND NOT "${CONFIG_WIFI_PASSWORD}" STREQUAL "")
    set(WIFI_PASSWORD_VAL "${CONFIG_WIFI_PASSWORD}")
endif()

# HA credentials: environment variables take priority over sdkconfig
set(HA_BASE_URL_VAL "")
if(DEFINED ENV{HA_BASE_URL} AND NOT "$ENV{HA_BASE_URL}" STREQUAL "")
    set(HA_BASE_URL_VAL "$ENV{HA_BASE_URL}")
elseif(DEFINED CONFIG_HA_BASE_URL AND NOT "${CONFIG_HA_BASE_URL}" STREQUAL "")
    set(HA_BASE_URL_VAL "${CONFIG_HA_BASE_URL}")
endif()

set(HA_TOKEN_VAL "")
if(DEFINED ENV{HA_TOKEN} AND NOT "$ENV{HA_TOKEN}" STREQUAL "")
    set(HA_TOKEN_VAL "$ENV{HA_TOKEN}")
elseif(DEFINED CONFIG_HA_TOKEN AND NOT "${CONFIG_HA_TOKEN}" STREQUAL "")
    set(HA_TOKEN_VAL "${CONFIG_HA_TOKEN}")
endif()

# Escape quotes for C string literals
string(REPLACE "\"" "\\\"" WIFI_SSID_ESC "${WIFI_SSID_VAL}")
string(REPLACE "\"" "\\\"" WIFI_PASSWORD_ESC "${WIFI_PASSWORD_VAL}")
string(REPLACE "\"" "\\\"" HA_BASE_URL_ESC "${HA_BASE_URL_VAL}")
string(REPLACE "\"" "\\\"" HA_TOKEN_ESC "${HA_TOKEN_VAL}")

idf_component_register(SRCS
    "main.c"
    "ui_style.c"
    "ui_icons.c"
    "ui_icons_font.c"
    "lcd_rgb_port.c"
    "lvgl_port.c"
    "src/config.c"
    "src/wifi_manager.c"
    # Home Assistant REST client and command queue (see ha_client.h and command_queue.h)
    "src/ha_client.c"       # HA REST client with GET/POST/PUT stubs, auth header helper, and verify_ssl config
    "src/command_queue.c"  # command queue worker skeleton
    "src/scene_service.c"  # scene and automation helpers
    "src/http_client.c"
    "src/discovery.c"
    "src/tabbed_ui.c"
    "src/offline_tracker.c"
    "src/ui_color_panel.c"
    "src/ui_scene_grid.c"
    "src/screen_manager.c"
    INCLUDE_DIRS "." "include"
    REQUIRES nvs_flash esp_http_client esp_wifi esp_system esp_event driver esp_timer esp_lcd esp_websocket_client cjson lvgl__lvgl)

target_compile_definitions(${COMPONENT_LIB} PRIVATE
    WIFI_SSID="${WIFI_SSID_ESC}"
    WIFI_PASSWORD="${WIFI_PASSWORD_ESC}"
    HA_BASE_URL="${HA_BASE_URL_ESC}"
    HA_TOKEN="${HA_TOKEN_ESC}"
)

idf_component_get_property(lvgl_lib lvgl__lvgl COMPONENT_LIB)
if(TARGET ${lvgl_lib})
    target_compile_options(${lvgl_lib} PRIVATE -Wno-format)
endif()

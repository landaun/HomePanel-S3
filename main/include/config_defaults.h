#pragma once

// Centralized defaults for panel configuration.
// Credentials are injected at compile time via CMakeLists.txt
// from either environment variables or sdkconfig (menuconfig).

// Compile-time credential macros (set by CMakeLists.txt)
#ifndef WIFI_SSID
#define WIFI_SSID ""
#endif

#ifndef WIFI_PASSWORD
#define WIFI_PASSWORD ""
#endif

#ifndef HA_BASE_URL
#define HA_BASE_URL ""
#endif

#ifndef HA_TOKEN
#define HA_TOKEN ""
#endif

#define PANEL_DEFAULT_WIFI_SSID WIFI_SSID
#define PANEL_DEFAULT_WIFI_PASSWORD WIFI_PASSWORD
#define PANEL_DEFAULT_HA_URL HA_BASE_URL
#define PANEL_DEFAULT_HA_TOKEN HA_TOKEN
#define PANEL_DEFAULT_USE_HTTPS false
#define PANEL_DEFAULT_UPDATE_INTERVAL_MS DEFAULT_UPDATE_INTERVAL_MS

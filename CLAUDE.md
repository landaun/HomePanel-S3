# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Project Overview

This is an **ESP32-S3 touch panel firmware** that implements a Wi-Fi connected Home Assistant control panel using ESP-IDF v5.5.0, LVGL graphics library, ST7701 RGB LCD driver (800x480), and GT911 capacitive touch controller.

**Target Hardware:** [Waveshare ESP32-S3-Touch-LCD-7](https://www.waveshare.com/esp32-s3-touch-lcd-7.htm) — ESP32-S3N16R8 (16MB flash, 8MB octal PSRAM), 7" IPS 800×480 RGB display (ST7701), GT911 5-point capacitive touch

## Build Commands

**Device Port:** `<YOUR_COM_PORT>` (e.g. `COM3` on Windows, `/dev/ttyUSB0` on Linux)

### Initial Setup

```bash
# Fetch submodule dependencies (required once after clone)
git submodule update --init --recursive

# Activate ESP-IDF environment (each terminal session)
# POSIX:
. $IDF_PATH/export.sh
# Windows CMD:
%IDF_PATH%\export.bat
# Windows PowerShell:
& $env:IDF_PATH\export.ps1

# Set ESP-IDF target (required once)
idf.py --preview set-target esp32s3

# Configure Home Assistant credentials (required once)
# Navigate to: Home Assistant > Home Assistant base URL / access token
idf.py menuconfig

# Clean and build
idf.py fullclean build

# Flash and monitor (replace with your port)
idf.py -p <YOUR_COM_PORT> flash monitor

# Exit monitor with Ctrl-]
```

### Testing

```bash
# Run unit tests
idf.py -T main build flash monitor
```

Unit tests use Unity framework and mock the HA client to avoid network dependencies. Test files are in `main/test/` and compiled with `UNIT_TEST` definition enabled.

### Configuration

```bash
# Configure project options
idf.py menuconfig
```

### Desktop Simulator (Windows)

Build and run the LVGL UI on a PC to iterate on layouts without flashing firmware:

```bash
# Prerequisites: CMake and Visual Studio Build Tools on PATH

# Configure
cmake -S simulator -B simulator/build

# Build
cmake --build simulator/build --config Release

# Run
.\simulator\build\Release\panel_simulator.exe
```

The simulator uses a small hardcoded discovery stub to populate the UI with generic entities.

## Architecture Overview

### Application Flow

```
app_main() -> panel_start_task (FreeRTOS, priority 5, core 1)
  +- config_init() + config_load()     # Load WiFi/HA credentials from NVS
  +- ha_client_setup()                  # Initialize HTTP client
  +- wifi_init() + esp_wifi_start()    # Connect to WiFi
  +- waveshare_esp32_s3_rgb_lcd_init() # Initialize ST7701 + GT911
  +- discovery_run() -> tabbed_ui_create()  # Fetch entities from HA, build tabbed UI
  +- start_background_tasks()           # Launch worker threads
```

### Background Tasks

| Task | Priority | Stack | Purpose | Interval |
| --- | --- | --- | --- | --- |
| `ha_discovery` | 4 | 12KB | Fetch entities from HA, build tabbed UI | One-shot |
| `ha_poll` | 3 | 8KB | Poll /api/states for entity state changes | 10s |
| `cmd_worker` | 5 | 4KB | Process command queue | On-demand |

### Key Subsystems

1. **Home Assistant Integration** (`ha_client.c`, `http_client.c`)
   - REST API wrapper with bearer token authentication
   - Polls `/api/states` every 10s for entity updates
   - Command queue with 120ms coalescing to batch rapid UI changes
   - Offline tracking: shows "OFFLINE" badge after 3 consecutive failures

2. **Auto-Discovery** (`discovery.c`)
   - Fetches all entities from `/api/states` at boot
   - Classifies entities by domain: light, climate, media_player, sensor (temperature), binary_sensor (occupancy), scene, automation, switch
   - Optional area filter via `CONFIG_HA_FILTER_AREA` menuconfig option
   - Caches discovered entities in NVS for fast subsequent boots
   - Result stored in `g_discovery` (`disc_result_t`) global

3. **Tabbed UI** (`tabbed_ui.c`, `ui_*.c`)
   - `lv_tileview`-based dashboard; one swipeable page per discovered domain
   - Only domains with at least one entity get a tab
   - Switch domain intentionally omitted (accidental toggle risk)
   - Status bar: WiFi and API indicators, battery percentage
   - Offline badge shown center-screen when HA unreachable
   - `update_status_indicators()` and `tabbed_ui_set_offline()` defined here

4. **Display Driver** (`waveshare_rgb_lcd_port.c`, `lvgl_port.c`)
   - ST7701 RGB parallel interface (800x480, 16-bit RGB565)
   - Double/triple-buffering in PSRAM to eliminate tearing
   - GT911 touch via I2C (400kHz, address 0x5D or 0x14)
   - LVGL port with frame buffer management and VSYNC callbacks

5. **Configuration & Storage** (`config.c`)
   - NVS namespace: `"panel_config"`
   - Credential priority: Environment variables -> NVS -> Compile-time defaults
   - WiFi, HA URL, HA token stored persistently

### Directory Structure

```
HomePanel-S3/
+-- main/
|   +-- main.c                         # Main event loop, WiFi, polling
|   +-- src/
|   |   +-- ha_client.c                # REST API wrapper
|   |   +-- http_client.c              # Low-level HTTP transport
|   |   +-- command_queue.c            # Async command execution
|   |   +-- discovery.c                # Entity fetch, classify, NVS cache
|   |   +-- tabbed_ui.c                # Swipeable tileview UI
|   |   +-- config.c                   # NVS storage
|   |   +-- wifi_manager.c             # WiFi connection handling
|   |   +-- offline_tracker.c          # API failure detection
|   |   +-- scene_service.c            # Scene activation, favorites
|   |   +-- ui_color_panel.c           # Color picker widget
|   |   +-- ui_scene_grid.c            # Scene grid widget
|   +-- include/                       # Header files
|   +-- test/                          # Unity unit tests
|   +-- ui_categories.json             # DEPRECATED - kept as reference only
|   +-- waveshare_rgb_lcd_port.c       # ST7701 + GT911 initialization
|   +-- lvgl_port.c                    # Frame buffer management
+-- components/
|   +-- lvgl__lvgl/                    # LVGL graphics library
|   +-- esp_lcd_touch_gt911/           # GT911 touch driver
|   +-- esp_websocket_client/          # WebSocket (not actively used)
|   +-- cjson/                         # JSON parsing
+-- simulator/                         # Desktop LVGL simulator (Windows)
|   +-- src/main.c                    # Simulator entry point
|   +-- stubs/                        # ESP-IDF API stubs
+-- scripts/
|   +-- set-env.ps1.template          # PowerShell credential loader template
+-- lv_conf.h                          # LVGL configuration
+-- sdkconfig.defaults                 # ESP-IDF defaults
+-- CMakeLists.txt                     # Build configuration
```

## Configuration System

### Credential Configuration (Recommended: menuconfig)

**For VSCode ESP-IDF Extension users**, use `idf.py menuconfig` (environment variables don't work reliably with the extension):

```powershell
# In VSCode terminal
& $env:IDF_PATH\export.ps1
idf.py menuconfig
```

Navigate to **Home Assistant** menu and set:

- **Home Assistant base URL**: `http://your-homeassistant:8123`
- **Home Assistant access token**: Your long-lived access token

Press `S` to save, `Q` to quit, then rebuild with `idf.py fullclean build`.

The sdkconfig file is gitignored, so credentials are safe from accidental commits.

### Credential Priority Order

1. **Environment variables** (highest priority, for CLI builds):

   ```bash
   export HA_BASE_URL="http://192.0.2.1:8123"
   export HA_TOKEN="your-long-lived-access-token"
   export WIFI_SSID="MyNetwork"
   export WIFI_PASSWORD="MyPassword"
   ```

2. **sdkconfig** (set via `idf.py menuconfig`):
   - `CONFIG_HA_BASE_URL`
   - `CONFIG_HA_TOKEN`

3. **NVS Flash** (persisted from previous runs):
   - Stored in namespace `"panel_config"`
   - Keys: `wifi_conf.ssid`, `wifi_conf.pass`, `ha_conf.url`, `ha_conf.token`

4. **Compile-time defaults** (lowest priority):
   - Edit `main/include/config_defaults.h`
   - Used as fallback when all other sources are absent

### Credential Compilation

Credentials are passed to the firmware via `main/CMakeLists.txt`:

```cmake
target_compile_definitions(${COMPONENT_LIB} PRIVATE
    HA_BASE_URL="${HA_BASE_URL_ESC}"
    HA_TOKEN="${HA_TOKEN_ESC}"
)
```

These macros are consumed by `config.c` during initialization.

### Entity Filtering (menuconfig)

Control which entities appear in the UI without editing C code. In `idf.py menuconfig` -> **Home Assistant**:

| Option | Description |
| --- | --- |
| `HA_FILTER_AREA` | Restrict display to a single HA area (leave blank for all) |
| `HA_ENTITY_SKIP_NUMBERED` | Hide lights whose name ends with a digit (e.g. "Light 1") |
| `HA_ENTITY_SKIP_LIGHT_KEYWORDS` | Comma-separated substrings to hide from Lights page |
| `HA_ENTITY_SKIP_TEMP_KEYWORDS` | Comma-separated substrings to hide from Temperature page |
| `HA_ENTITY_SKIP_OCC_KEYWORDS` | Comma-separated substrings to hide from Occupancy page |

## Hardware Configuration

### Display Timings

Adjust GPIO assignments and timings in `main/waveshare_rgb_lcd_port.h` to match your hardware.

### Touch Controller

GT911 communicates over I2C. Pin assignments are in `main/waveshare_rgb_lcd_port.h` (pulled from `sdkconfig` macros). Default configuration:

- SCL: GPIO 9 (`I2C_MASTER_SCL_IO`)
- SDA: GPIO 8 (`I2C_MASTER_SDA_IO`)
- RST: GPIO 4 (`TOUCH_RST_GPIO`) — toggled during startup
- INT: GPIO -1 (unused, `EXAMPLE_PIN_NUM_TOUCH_INT`)

### PSRAM Configuration

Frame buffers (760KB) are allocated in PSRAM. Ensure `CONFIG_SPIRAM=y` in `sdkconfig.defaults`:

```
CONFIG_SPIRAM=y
CONFIG_SPIRAM_MODE_OCT=y
CONFIG_SPIRAM_SPEED_80M=y
```

## Important Patterns

### LVGL Thread Safety

All LVGL object modifications must be protected by the LVGL mutex:

```c
lvgl_port_lock(timeout_ms);
// ... modify LVGL objects ...
lvgl_port_unlock();
```

### Command Queue Pattern

User interactions enqueue commands asynchronously to avoid blocking the UI:

```c
// From LVGL callback
command_queue_enqueue_light_brightness(entity_id, brightness);

// cmd_worker task processes in background
// Coalesces duplicate commands within 120ms
```

### State Updates

HA state changes are polled every 10s and update the UI via `tabbed_ui_update_*()`:

```c
// ha_poll_task -> parses /api/states response
tabbed_ui_update_light(slot, is_on, brightness);
tabbed_ui_update_temperature(slot, temp_celsius);
tabbed_ui_update_occupancy(slot, is_occupied);
tabbed_ui_update_media(slot, state, volume, media_title);
```

`slot` is the zero-based index of the entity within its domain (as discovered by `discovery.c`).
Skipped entities still increment the slot counter to keep poll indices aligned.

### Error Handling

- Network failures -> Offline tracker counts failures, shows "OFFLINE" badge after 3 attempts
- Resource allocation failures -> Fallback to defaults, log error, continue execution
- Missing JSON -> Log and skip entity
- NVS failures -> Use compile-time defaults

### Memory Management

- Frame buffers -> **PSRAM** (`fb_in_psram=1`)
- Entity and widget arrays -> **PSRAM** (`MALLOC_CAP_SPIRAM`)
- Command queue, JSON parsing -> **SRAM** (temporary, freed after use)

## Key Files Reference

| File | Responsibility |
| --- | --- |
| `main/main.c` | Main event loop, WiFi handling, HA polling (~500 lines) |
| `main/src/discovery.c` | Entity fetch, domain classification, NVS cache |
| `main/src/tabbed_ui.c` | Swipeable tileview UI, per-domain page builders, state updates |
| `main/src/ha_client.c` | REST API wrapper, service calls |
| `main/src/command_queue.c` | Async command execution, 120ms coalescing |
| `main/src/config.c` | NVS storage, credential loading |
| `main/waveshare_rgb_lcd_port.c` | ST7701 + GT911 initialization |
| `main/lvgl_port.c` | Frame buffer management, VSYNC callbacks |

## Critical sdkconfig Options

```
CONFIG_ESP_DEFAULT_CPU_FREQ_MHZ_240=y         # 240MHz CPU for UI performance
CONFIG_SPIRAM=y                                # Enable PSRAM for frame buffers
CONFIG_FREERTOS_HZ=1000                        # 1ms tick for responsive UI
CONFIG_ESP32S3_DATA_CACHE_LINE_64B=y           # Optimize cache for large buffers
CONFIG_EXAMPLE_LVGL_PORT_AVOID_TEAR_ENABLE=y   # Double-buffering anti-tearing
CONFIG_LV_MEM_CUSTOM=y                         # Custom LVGL memory allocator
CONFIG_MBEDTLS_CERTIFICATE_BUNDLE=y            # TLS cert bundle for HTTPS
```

## REST API vs WebSocket

This project uses **REST API polling** instead of WebSocket subscriptions:

- Simpler implementation, no subscription management complexity
- 10-second polling interval provides acceptable latency for a control panel
- Trade-off: Higher bandwidth (~1KB/10s) vs real-time updates

WebSocket client is included in `components/esp_websocket_client/` but not actively used.

## Common Pitfalls & Best Practices

### LVGL String Formatting

**Use `snprintf()` + `lv_label_set_text()` instead of `lv_label_set_text_fmt()`** to avoid crashes with certain UTF-8 strings:

```c
// GOOD
char buffer[128];
snprintf(buffer, sizeof(buffer), "%s: %.1f F", name, temp);
lv_label_set_text(label, buffer);
```

### NULL Pointer Safety in Widget Arrays

Always check both the count AND the array pointer before accessing widget slots:

```c
// GOOD - check both count AND pointer
if (slot >= s_widget_counts[domain] || !s_widgets[domain]) return;
tab_widget_t* w = &s_widgets[domain][slot];
```

Widget arrays are allocated in PSRAM via `heap_caps_calloc()`. If allocation fails, the count may be set but the pointer remains NULL.

### Slot Alignment for Filtered Entities

When an entity is filtered out (e.g. by `light_should_skip()`), the slot counter must still increment so that the poll task slot indices remain aligned with discovery order:

```c
if (light_should_skip(e->friendly_name)) { slot++; continue; }
```

Skipped slots have NULL widget pointers; all `tabbed_ui_update_*()` functions guard against this.

## Release Checklist

- [ ] Version bumped and `CHANGELOG.md` updated
- [ ] `sdkconfig.defaults` regenerated: `idf.py menuconfig && idf.py save-defconfig`
- [ ] Git tag created and pushed
- [ ] `idf.py fullclean build` succeeds from clean clone
- [ ] Smoke test run on hardware (24h soak preferred)

## ESP-IDF Version Requirement

**Requires ESP-IDF v5.5.0 exactly.** Earlier versions lack required RGB LCD APIs.

Verify version:

```bash
idf.py --version
```

Install specific version:

```bash
git clone -b v5.5.0 --recursive https://github.com/espressif/esp-idf.git
cd esp-idf
./install.sh esp32s3
. ./export.sh
```

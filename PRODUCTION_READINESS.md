# Production Readiness

## Hardware Requirements

- ESP32-S3 module or devkit
- 7" RGB LCD with capacitive touch (e.g., Waveshare ESP32-S3-Touch-LCD-7)
- Reliable 5V power supply capable of 2A peak
- Stable Wi-Fi network with Home Assistant reachable on the local network

## Build

Requires [ESP-IDF v5.5](https://github.com/espressif/esp-idf/tree/v5.5). Activate the ESP-IDF
environment and run a clean build:

```bash
idf.py --preview set-target esp32s3
idf.py fullclean build
```

## Configure

Set credentials via `idf.py menuconfig` before building (navigate to **WiFi Configuration** and
**Home Assistant**), or export them as environment variables and rebuild:

```bash
export WIFI_SSID="MyNetwork"
export WIFI_PASSWORD="MyPassword"
export HA_BASE_URL="http://homeassistant.local:8123"
export HA_TOKEN="your-long-lived-access-token"
idf.py fullclean build
```

Credentials are stored in NVS after first boot; subsequent reboots use cached values.

## Flash

```bash
idf.py -p <PORT> flash monitor
```

Replace `<PORT>` with your serial port (e.g. `/dev/ttyUSB0` on Linux, `COM3` on Windows).

## Usage

After connecting to Wi-Fi the panel auto-discovers all Home Assistant entities and builds a
swipeable tabbed dashboard — one page per domain (Lights, Temperature, Climate, Occupancy,
Media, Scenes). Swipe left/right or tap the tab buttons to navigate. No manual entity
configuration is required.

## Reliability Checks

- Verify the panel runs for 24 hours without watchdog resets or memory leaks.
- Confirm reconnect logic handles Wi-Fi or Home Assistant outages (offline badge should appear
  after 3 consecutive failures, then clear on reconnect).
- Exercise all UI pages and verify entity state updates arrive within ~10 seconds.
- Exercise touch accuracy across the full display area.

## Monitoring

Use `idf.py monitor` to view log output. Key log tags:

| Tag | Module |
| --- | --- |
| `discovery` | Entity fetch and classification |
| `tabbed_ui` | UI construction and updates |
| `ha_client` | REST API calls |
| `CMDQ` | Command queue worker |
| `wifi_mgr` | Wi-Fi events |

# Troubleshooting

## Logging

Adjust the log level via menuconfig (`Component config → Log output → Default log level`) or define `LOG_LOCAL_LEVEL` for per-file control. Use `idf.py monitor` to view runtime logs.

## Watchdog

The main task watchdog resets the device if tasks hang. Check for `WDT timeout` logs and ensure long operations yield or run in separate tasks.

## Heap Tips

Use `heap_caps_get_free_size(MALLOC_CAP_DEFAULT)` or `idf.py size-components` to monitor memory. Free unused LVGL objects promptly.

## Offline Behavior

If Wi-Fi or Home Assistant is unreachable, the panel retries periodically and shows an offline icon. Verify network credentials and HA URL.

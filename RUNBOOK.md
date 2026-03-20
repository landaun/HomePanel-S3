# Runbook

> Requires ESP-IDF v5.5.0

This runbook summarizes how to build and flash the ESP32-S3 touch panel firmware.

1. Install [ESP-IDF v5.5.0](https://github.com/espressif/esp-idf/tree/v5.5.0) and export the build environment.
2. Build the project:

   ```bash
   with-idf idf.py --preview set-target esp32s3
   with-idf idf.py fullclean build
   ```

3. Flash the firmware and start the monitor:

   ```bash
   with-idf idf.py -p <PORT> flash monitor
   ```

   Exit the monitor with `Ctrl-]`.

## Summary

<!-- Briefly describe what this PR changes and why. -->

## Hardware/Target

* [ ] Target: **ESP32-S3**
* [ ] Display/Touch affected? (e.g., RGB panel, GT911/CSTxxx)
* [ ] Other peripherals affected? (SPI, I2C, SD, Wi‑Fi)

## How to Build

```bash
with-idf idf.py --preview set-target esp32s3
with-idf idf.py fullclean build
```

## Screenshots / Logs (optional)

<!-- Attach relevant images, serial logs, or size reports (idf.py size-components). -->

## Risks

<!-- Timing/ISR changes, memory/PSRAM usage, task priorities, Wi‑Fi/HTTP behavior, LVGL config changes, etc. -->

## Testing Notes

<!-- Explain how you validated these changes (commands, hardware, logs). -->

## Checklist

<!-- Confirm all items before requesting review. -->

* [ ] Small, focused commits with clear messages
* [ ] CI green (build succeeds)
* [ ] README/Docs updated if user‑facing behavior changed
* [ ] Documented any `menuconfig` changes (if applicable)
* [ ] No unrelated file changes

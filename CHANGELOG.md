# Changelog

## [1.0.0] - 2026-03-20

### Added

- Auto-discovery of Home Assistant entities at boot (lights, temperature, climate, occupancy, media, scenes/automations)
- Swipeable tabbed dashboard — one full-screen page per discovered domain
- Inline climate ±1°F setpoint controls
- Media play/pause toggle and volume slider with now-playing title
- Area filter and keyword exclusion filters via `idf.py menuconfig` — no code changes needed
- NVS caching of discovered entities for fast boot
- Offline badge after 3 consecutive HA API failures, auto-clears on reconnect
- Desktop LVGL simulator for Windows (no hardware required for UI iteration)
- Async command queue with 120ms coalescing for responsive touch interaction

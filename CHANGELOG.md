# Changelog

## [Unreleased]

### Breaking Changes

- Replaced `panel_ui.c` (static JSON-configured UI) with `tabbed_ui.c` + `discovery.c` (auto-discovery, swipeable tileview). Entity configuration via `ui_categories.json` is no longer used.

### Added

- `discovery.c` — fetches all HA entities from `/api/states` at boot, classifies by domain (lights, temperature, climate, occupancy, media, scenes/automations), caches in NVS for fast reboot
- `tabbed_ui.c` — swipeable `lv_tileview`-based dashboard with one page per discovered domain; inline climate ±1°F controls; media play/pause + volume slider + now-playing title
- Menuconfig entity filters: hide specific entities by keyword or numbered suffix without editing C code
- Offline badge displayed center-screen after 3 consecutive HA API failures

### Removed

- `panel_ui.c` / `panel_ui.h` — 1,500+ lines of legacy static UI code
- `ui_categories.json` embed from firmware binary (file kept in repo as deprecated reference)
- Switch domain UI page — intentionally omitted (smart plugs, accidental toggle risk)
- `ha_entities.json` from repo — was a private local snapshot, not part of the project
- `.claude/` directory — internal AI tooling, not for public repos

### Security

- `SECURITY.md` updated to use GitHub Security Advisories instead of unmonitored email

## [0.1.2] - 2025-08-25

- Removed legacy documentation files and directories (`AGENTS.md`, `DOCS_AUDIT.md`, `DOCS_EXECUTION_LOG.md`, `DOCS_STATE.json`, `docs/`, and `codex/`).

## [0.1.1] - 2025-08-22

- Updated documentation and removed placeholder entries.

## [0.1.0] - 2025-02-14

- Initial public release with production-ready documentation.

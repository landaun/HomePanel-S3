# Touch Panel UI Redesign Plan

## Goal
Transform the current text-only layout into a polished, Home Assistant–style dark-mode interface while preserving existing data flows and preparing for future interactive controls.

## Design Direction
- Adopt Home Assistant’s dark palette: charcoal background, slightly lighter cards, and accent hues (teal, amber, lime, coral).
- Favor a clean, modular layout with stacked sections, consistent typography, and room for future controls.
- Keep the UI text-based for now but structure it so toggles/sliders can be added later without redesign.

## Implementation Steps

### 1. Foundations & Styles
- Add a dark-mode style set in `ui_style.c` with:
  - Screen background color
  - Default font/color pairings
  - Card background & spacing
  - Accent text colors
- Ensure styles are applied after `lvgl_port_init`.
#### Subtasks
- [x] Audit current `ui_style.c` definitions and identify reusable elements.
- [x] Define dark palette constants (background, card, text, accent).
- [x] Implement helper functions for new styles (screen, card, headers, values).
- [x] Hook style init into existing startup path.

### 2. Structural Layout
- Replace the flat list with a single scrollable flex column (`LVGL` flex).
- Maintain tight vertical spacing while preserving readability.
- Confirm scroll behaviour covers all categories on-device.
#### Update
- Add support for two-column layouts on wide screens while retaining single-column fallback.
#### Subtasks
- [x] Outline target hierarchy (screen -> scroll container -> sections -> entries).
- [x] Refactor `panel_ui_activate` to create the new container layout.
- [ ] Validate scroll/padding behaviour in simulator/device.
- [x] Implement responsive two-column arrangement (left/right columns or grid).

### 3. Section Headers & Cards
- Introduce section headers (Favorites, Lighting, Temperature, Occupancy, Scenes).
- Wrap entries in lightly elevated cards (rounded corners, subtle shadow).
- For dense data (e.g., favorites), consider multi-column flex arrangements.
- [x] Design card/header style variants leveraging dark palette.
- [x] Update builder functions to create section wrappers and apply styles.
- [ ] Evaluate small-screen behaviour; adjust flex properties as needed.

### 4. Status Bar Refresh
- Restyle the top status bar for dark mode.
- Adjust indicator colors (Wi-Fi/API) to small colored badges while keeping overall muted look.
#### Subtasks
- [x] Define status-bar-specific styles (background, label, indicators).
- [x] Apply new styles inside `configure_status_bar`.
- [x] Review icon colors for contrast against dark background.

### 5. Typography & Icons
- Set consistent font sizes and weights.
- Use HA-aligned iconography (Font Awesome glyphs or HA symbols) for each data category.
- Apply state-based colors (e.g., warm amber for lights on, cool blue for temperatures).
#### Subtasks
- [x] Inventory current fonts/icons packaged with firmware.
- [x] Map each data type to icon + color pair.
- [x] Implement helper to apply typography/icon styles within entry builders.

### 6. Data Presentation
- Within each card, display:
  - Entity name (muted label)
  - Primary value (bold, bright)
  - Secondary state text (smaller, subdued)
- Group lights or temperatures logically if needed.
#### Subtasks
- [x] Decide per-entry layout (two-line labels with name/value).
- [ ] Update data update functions (`panel_ui_update_*`) to format text accordingly.
- [x] Add utility to format temperature/occupancy strings uniformly.

### 7. Scenes & Automations
- (Favorites section removed per latest requirements.)
- List scenes/automations with enabled/disabled state cues and optional trigger icons.
#### Subtasks
- [ ] (n/a) — favorites removed.
- [ ] Style scenes/automations entries with state badges or icons.
- [ ] Ensure lists remain scrollable and visually consistent with other sections.

### 8. Loading & Error States
- Replace plain “Updating…” text with lighter styling or a small spinner.
- Use italic, muted gray text for fallback messages (no data, offline).
#### Subtasks
- [ ] Design loading/error text styles (font + color).
- [ ] Update `panel_ui_set_loading` to apply styles or spinner widget.
- [ ] Audit all fallback paths to confirm messaging matches design.

### 9. Offline Overlay & Toasts
- Restyle offline badge and toast notifications to match dark theme (e.g., semi-transparent red overlay).
- Ensure they contrast without being visually harsh.
#### Subtasks
- [ ] Update offline badge style (background, text, opacity).
- [ ] Theme toast notifications for dark mode.
- [ ] Test visibility/readability under various lighting conditions.

### 10. Polish & Responsiveness
- Test on hardware: adjust padding, card widths, and font sizes to ensure readability.
- Verify the bottom sections (e.g., Scenes) are reachable and not hidden by scroll constraints.
#### Subtasks
- [ ] Run simulator + device tests, collect notes on spacing/legibility.
- [ ] Iterate on padding/flex values until layout feels balanced.
- [ ] Capture before/after screenshots for documentation.

### 11. Phase 2 Preparation
- Leave structural placeholders for future interactivity (toggles, sliders, buttons).
- Keep code modular so interactive widgets can be dropped into the existing card layout later.
#### Subtasks
- [ ] Identify where interactive controls will live within each card.
- [ ] Stub helper functions/hooks for future toggles/sliders.
- [ ] Document integration points in code comments or separate notes.

## Tracking
- [ ] Styles implemented in `ui_style.c`
- [ ] Scrollable column layout created
- [ ] Section headers & cards styled
- [ ] Status bar themed
- [ ] Typography & icons unified
- [ ] Data presentation polished
- [ ] Favorites/scenes rendered cleanly
- [ ] Loading/offline states themed
- [ ] Device testing adjustments made
- [ ] Interactive placeholders ready

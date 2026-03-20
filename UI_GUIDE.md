# UI Guide

## Layout

The panel shows a fixed status bar at the top, a row of tab buttons below it, and a full-screen
content area that you swipe between.

```
+----------------------------------------------------------+
|  WiFi: Connected    HA: Connected           [bat 94%]    |  <- status bar (30px)
+--[ Lights ]--[ Temp ]--[ Climate ]--[ Media ]--[ Scenes ]-+  <- tab header (44px)
|                                                          |
|                   <page content>                         |  <- tileview (rest of screen)
|                                                          |
+----------------------------------------------------------+
```

Only tabs with at least one discovered entity are shown. Tabs are scrollable if there are many.

## Navigation

- **Swipe left/right** on the content area to move between pages.
- **Tap a tab button** to jump directly to that page.
- The active tab is highlighted in teal; inactive tabs are dimmed.

## Pages

### Lights

Each light gets a card with:

- Toggle switch (on/off)
- Brightness slider (0–100%)
- Entity name

Tap the toggle to turn the light on or off. Drag the slider to set brightness; the command is
sent when you release.

### Temperature

Read-only sensor cards showing the current temperature from each sensor entity. Values update
every ~10 seconds.

### Climate

Each thermostat gets a card with:

- Current temperature and setpoint displayed together (e.g. `Now: 70.2  Set: 72 F`)
- `[ - ]  72  [ + ]` inline controls to adjust the setpoint ±1°F (range: 60–85°F)

Tap `-` or `+` to adjust the setpoint. The command is sent immediately and the label updates
without waiting for the next poll.

### Occupancy

Read-only binary sensor cards showing "Occupied" or "Vacant" for each motion/occupancy/presence
sensor.

### Media

Each media player gets a card with:

- Speaker name
- Now-playing title (or blank when idle)
- `[ > ]` / `[ || ]` play/pause button
- Volume slider (0–100%)

Tap the play/pause button to toggle playback. Drag the volume slider to set volume.

### Scenes & Automations

Each scene and automation gets a button card. Tap to activate the scene or trigger the
automation. Long-press to toggle its favorite status (stored in NVS, persists across reboots).

## Status Bar

| Indicator | Meaning |
| --- | --- |
| WiFi dot (teal) | Connected to Wi-Fi |
| WiFi dot (red) | Wi-Fi disconnected |
| HA dot (teal) | Home Assistant API reachable |
| HA dot (red) | Home Assistant unreachable |
| Battery % | Current battery level (if hardware fitted) |

## Offline Badge

If Home Assistant fails to respond for 3 consecutive polls (~30 seconds), a large red
**"Offline"** badge appears in the center of the screen. The badge clears automatically when
connectivity is restored.

## Notes

- **Switches are not shown.** Smart plugs are intentionally excluded to prevent accidental
  toggling (e.g. toggling a device mid-cycle).
- Commands are coalesced over 120ms — rapid taps on a slider send only one command.
- Entity states update approximately every 10 seconds.

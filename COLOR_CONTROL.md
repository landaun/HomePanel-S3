# Color Control

## mireds ↔ Kelvin

Color temperature uses mireds. Convert Kelvin to mireds with:

```text
mireds = round(1e6 / kelvin)
```

Clamp the result within device `min_mireds` and `max_mireds` values.

## supported_color_modes

Home Assistant exposes `supported_color_modes` for each light. The panel enables RGB or color temperature widgets only when the mode list contains `"rgb"` or `"color_temp"`.

## Debounce

UI sliders send commands after a short debounce to avoid flooding the network. Increase the delay for noisy sensors or slow networks.

# FAQ

## Why doesn't the panel connect to Home Assistant?

Ensure `HA_BASE_URL` and `HA_TOKEN` match your server and the device has network connectivity. An offline icon appears when unreachable.

## How do I update firmware?

Rebuild and flash with:

```bash
idf.py -p <PORT> flash monitor
```

Exit the monitor with `Ctrl-]`.

## Can the panel run without Home Assistant?

No. It relies on the Home Assistant HTTP API for entity data.

## Touch input is unresponsive—what should I check?

Confirm the GT911 touch controller wiring and I2C pins in `main/`.

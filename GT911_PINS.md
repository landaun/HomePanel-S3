# GT911 Touch Controller Pins

The GT911 capacitive controller communicates over I2C and exposes dedicated control lines. Pin assignments are configured in [`main/waveshare_rgb_lcd_port.h`](main/waveshare_rgb_lcd_port.h).

## I2C Lines

- **SCL**: `I2C_MASTER_SCL_IO` (`CONFIG_EXAMPLE_I2C_SCL_IO`)
- **SDA**: `I2C_MASTER_SDA_IO` (`CONFIG_EXAMPLE_I2C_SDA_IO`)

## Control Lines

- **Reset**: `EXAMPLE_PIN_NUM_TOUCH_RST` / `TOUCH_RST_GPIO`
- **Interrupt**: `EXAMPLE_PIN_NUM_TOUCH_INT`

These macros pull their values from `sdkconfig` options, allowing the pins to be adjusted for different boards. The reset line is toggled during startup in [`main/waveshare_rgb_lcd_port.c`](main/waveshare_rgb_lcd_port.c).

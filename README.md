# Pi Lab

A Raspberry Pi environment monitor that reads temperature and humidity from a Sensirion SHT35 sensor and displays live readings on an ST7735 TFT LCD.

## Hardware

| Component | Interface | Default pin / address |
|-----------|-----------|----------------------|
| SHT35 temp/humidity sensor | I2C (bus 1) | `0x44` |
| ST7735 1.8" TFT display (128×160) | SPI (`/dev/spidev0.0`) | DC: GPIO 24, RST: GPIO 25 |
| Backlight (optional) | Hardware PWM | GPIO 18 (PWM0, `dtoverlay=pwm`) |

## Features

- **Live display** — temperature and humidity refreshed every second on the ST7735
- **Console output** — same readings printed to stdout with a timestamp
- **File logging** — appends to `logs/monitor.log` in the format `[YYYY-MM-DD HH:MM:SS] temp=XX.XC humidity=XX.X% status=OK`
- **Alert** — status turns `ALERT` (red on display) when temperature exceeds 30 °C
- **Clean shutdown** — handles `SIGINT` / `SIGTERM`; clears the display before exiting

### Display layout

```
y=  8  Temp:          (label)
y= 26   25.3C         (large white text, scale 3)
y= 55  Hum:           (label)
y= 73   62.1%         (large cyan text, scale 3)
y=102  OK             (green) / ALERT (red)
y=124  17:28:47       (time)
y=134  2026-05-20     (date)
```

## Raspberry Pi setup

### I2C (SHT35)

1. Enable I2C in `raspi-config`.
2. Confirm the sensor is visible: `i2cdetect -y 1`
3. Wire SHT35 to `3.3V`, `GND`, `SDA`, `SCL`.

### SPI (ST7735)

1. Enable SPI in `raspi-config`.
2. Wire the display: `VCC→3.3V`, `GND`, `SCL→SCLK`, `SDA→MOSI`, `RES→GPIO25`, `DC→GPIO24`, `CS→CE0`.

### Backlight PWM (optional)

Add to `/boot/firmware/config.txt`:

```
dtoverlay=pwm,pin=18,func=2
```

## Build and run

```sh
make build
sudo ./pi_lab
```

`sudo` is required for GPIO and SPI access.

## Environment variables

| Variable | Default | Description |
|----------|---------|-------------|
| `SHT35_I2C_BUS` | `1` | I2C bus number |
| `SHT35_I2C_ADDR` | `0x44` | I2C address (`0x44` or `0x45`) |
| `ST7735_SPI_DEV` | `/dev/spidev0.0` | SPI device path |
| `ST7735_DC_PIN` | `24` | Data/command GPIO pin |
| `ST7735_RST_PIN` | `25` | Reset GPIO pin |
| `ST7735_PWM_CHIP` | `/sys/class/pwm/pwmchip0` | PWM chip path |
| `ST7735_BL_PIN` | `18` | Backlight GPIO pin |

## Project structure

```
src/
  main.c          — main loop, display update
  sensor_sht35.c  — SHT35 I2C driver
  st7735.c        — ST7735 SPI driver + 5×7 bitmap font
  logger.c        — file logger
  alert.c         — threshold alert logic
include/
  sensor_sht35.h
  st7735.h
  logger.h
  alert.h
logs/
  monitor.log
scripts/
  sync_pi.sh      — rsync from Mac to Pi
  sync_mac.sh     — rsync from Pi to Mac
```

## Development

The project cross-compiles on macOS; non-Linux stubs are provided for `sensor_sht35.c` and `st7735.c` so `make build` succeeds on both platforms. Use the sync scripts to push changes between machines.

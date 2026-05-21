# Pi Lab

A Raspberry Pi 5 environment monitor that reads temperature and humidity from a Sensirion SHT35 sensor, receives CPU temperature from a Raspberry Pi Pico 2 via UART, and displays live readings on an ST7735 TFT LCD.

## Hardware

| Component | Interface | Default pin / address |
|-----------|-----------|----------------------|
| SHT35 temp/humidity sensor | I2C (bus 1) | `0x44` |
| ST7735 1.8" TFT display (128×160) | SPI (`/dev/spidev0.0`) | DC: GPIO 24, RST: GPIO 25 |
| Backlight (optional) | Hardware PWM | GPIO 18 (PWM0, `dtoverlay=pwm`) |
| Raspberry Pi Pico 2 | UART (`/dev/ttyAMA0`) | TX: GPIO 0 → Pi GPIO 15, RX: GPIO 1 → Pi GPIO 14 |

### Pico 2 wiring

| Pico 2 pin | Pi 5 pin | Function |
|------------|----------|----------|
| Pin 1 (GP0, TX) | Pin 10 (GPIO 15, RX) | UART data |
| Pin 2 (GP1, RX) | Pin 8 (GPIO 14, TX) | UART data |
| Pin 38 (GND) | Pin 6 (GND) | Ground |
| Pin 39 (VSYS) | Pin 2 (5V) | Power (optional, if not using USB) |

## Features

- **Live display** — temperature, humidity, and Pico 2 CPU temperature refreshed every loop on the ST7735
- **Console output** — readings printed to stdout with a timestamp every 20 seconds
- **File logging** — appends to `logs/monitor.log` in the format `[YYYY-MM-DD HH:MM:SS] temp=XX.XC humidity=XX.X% status=OK`
- **Alert** — status turns `ALERT` (red on display) when SHT35 temperature exceeds 30 °C
- **Pico 2 CPU temp** — received via UART as `TEMP:XX.X\n`; shows `---` if unavailable
- **Clean shutdown** — handles `SIGINT` / `SIGTERM`; clears the display before exiting

### Display layout

```
y=  2  SHT35          (label, gray)
y= 12   25.3C         (white, scale 2)
y= 28   62.1%         (cyan, scale 2)
y= 50  PICO2 CPU      (label, gray)
y= 60   45.2C         (yellow, scale 2) / --- if no data
y= 82  OK             (green) / ALERT (red)
y=104  17:28:47       (time, gray)
y=114  2026-05-20     (date, gray)
```

## Raspberry Pi 5 setup

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

### UART (Pico 2)

Add to `/boot/firmware/config.txt`:

```
dtoverlay=uart0-pi5
```

The Pico 2 runs `pico2/bmc_sim.c` which sends `TEMP:XX.X\n` at 115200 baud every second over UART0 (GP0/GP1). The Pi reads this on `/dev/ttyAMA0`.

## Build and run

```sh
make build
sudo ./pi_lab
```

`sudo` is required for GPIO and SPI access.

### Pico 2 firmware

```sh
cd pico2
mkdir build && cd build
cmake ..
make
```

Flash `bmc_sim.uf2` to the Pico 2 by holding BOOTSEL and copying the file to the USB mass storage device.

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
| `PICO_UART_DEV` | `/dev/ttyAMA0` | Pico 2 UART device |
| `PICO_UART_BAUD` | `115200` | Pico 2 UART baud rate |

## Project structure

```
src/
  main.c          — main loop, display update
  sensor_sht35.c  — SHT35 I2C driver
  sensor_pico.c   — Pico 2 UART reader
  st7735.c        — ST7735 SPI driver + 5×7 bitmap font
  logger.c        — file logger
  alert.c         — threshold alert logic
include/
  sensor_sht35.h
  sensor_pico.h
  st7735.h
  logger.h
  alert.h
pico2/
  bmc_sim.c       — Pico 2 firmware (simulated BMC CPU temperature)
  CMakeLists.txt
logs/
  monitor.log
scripts/
  sync_pi.sh      — rsync from Mac to Pi
  sync_mac.sh     — rsync from Pi to Mac
```

## Development

The project cross-compiles on macOS; non-Linux stubs are provided for `sensor_sht35.c` and `st7735.c` so `make build` succeeds on both platforms. Use the sync scripts to push changes between machines.

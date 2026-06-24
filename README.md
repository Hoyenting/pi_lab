# Pi Lab — Embedded BMC Monitor

[Project Presentation (PDF)](docs/PiLabBMC.pdf)

A Raspberry Pi 5 environment monitor that emulates a subset of BMC (Baseboard Management Controller) functionality: multi-sensor thermal monitoring, a Redfish-inspired REST API, an IPMI-style System Event Log, and a fan-speed simulation loop driven by a Raspberry Pi Pico 2 acting as the BMC firmware.

## Architecture

```
┌──────────────────────────────────────────────────────────────┐
│                   Raspberry Pi 5  (Host OS)                   │
│                                                              │
│  ┌─────────────┐  ┌────────────────┐  ┌──────────────────┐  │
│  │  SHT35      │  │  ST7735 TFT    │  │  Redfish HTTP    │  │
│  │  I2C driver │  │  SPI driver    │  │  API  (:8080)    │  │
│  └──────┬──────┘  └───────┬────────┘  └────────┬─────────┘  │
│         │                 │                     │            │
│  ┌──────▼─────────────────▼─────────────────────▼─────────┐  │
│  │                    Main monitor loop                    │  │
│  │   alert_check()  →  SEL  →  logger  →  display_update  │  │
│  └─────────────────────────────────────────────────────────┘  │
└────────────────────────────┬─────────────────────────────────┘
                             │ UART 115200 baud
┌────────────────────────────▼─────────────────────────────────┐
│              Raspberry Pi Pico 2  (BMC Simulator)             │
│                                                              │
│   ADC → on-chip temp sensor                                  │
│   Fan curve: linear RPM ∝ temperature (800–4000 RPM)         │
│   PWM → GP2 (open-loop fan tachometer signal)                │
│   UART TX: "BMC:TEMP:XX.X,FAN:XXXX\n" every 1 s             │
└──────────────────────────────────────────────────────────────┘
```

## BMC Design Concepts Demonstrated

| BMC Feature | Implementation |
|-------------|----------------|
| Thermal sensor polling | SHT35 via I2C, Pico 2 ADC via UART |
| Multi-level thresholds | WARNING / CRITICAL (IPMI Upper Non-Critical / Critical) |
| System Event Log (SEL) | State-transition–only writes to `logs/sel.log` |
| Fan speed control | Linear fan curve in `pico2/bmc_sim.c`, PWM on GP2 |
| Out-of-band API | Redfish v1 stub: `/redfish/v1/Chassis/1/Thermal` |
| Structured logging | Severity-stamped log lines in `logs/monitor.log` |
| Configuration | `config.ini` with env-var override (same priority model as OpenBMC) |

## Hardware

| Component | Interface | Default pin / address |
|-----------|-----------|----------------------|
| SHT35 temp/humidity sensor | I2C (bus 1) | `0x44` |
| ST7735 1.8" TFT display (128×160) | SPI (`/dev/spidev0.0`) | DC: GPIO 24, RST: GPIO 25 |
| Backlight (optional) | Hardware PWM | GPIO 18 (PWM0, `dtoverlay=pwm`) |
| Raspberry Pi Pico 2 | UART (`/dev/ttyAMA0`) | TX: GPIO 0 → Pi GPIO 15 |

### Pico 2 wiring

| Pico 2 pin | Pi 5 pin | Function |
|------------|----------|----------|
| Pin 1 (GP0, TX) | Pin 10 (GPIO 15, RX) | UART data |
| Pin 2 (GP1, RX) | Pin 8 (GPIO 14, TX) | UART data |
| Pin 4 (GP2) | — | Fan PWM signal (optional scope probe) |
| Pin 38 (GND) | Pin 6 (GND) | Ground |
| Pin 39 (VSYS) | Pin 2 (5V) | Power (optional, if not using USB) |

## Features

- **Live display** — SHT35 temp/humidity, Pico 2 CPU temp, fan RPM, and alert status on ST7735
- **Three-level alerts** — `OK` / `WARN` / `CRIT` driven by configurable thresholds
- **System Event Log** — IPMI-style SEL written on state transitions to `logs/sel.log`
- **Redfish-inspired API** — HTTP endpoint at `:8080` serves JSON sensor and event data
- **Structured logging** — severity-stamped entries in `logs/monitor.log`
- **Fan simulation** — Pico 2 runs a linear fan curve and emits RPM over UART
- **Clean shutdown** — handles `SIGINT` / `SIGTERM`, clears display, closes SEL

### Display layout

```
y=  2  SHT35            (label, gray)
y= 12   25.3C           (white, scale 2)
y= 28   62.1%           (cyan, scale 2)
y= 48  PICO2 CPU        (label, gray)
y= 58   45.2C           (yellow, scale 2) / --- if no data
y= 76  FAN 2400RPM      (gray, scale 1)   / FAN --- if no data
y= 86  OK               (green) / WARN (yellow) / CRIT (red)
y=108  17:28:47         (time, gray)
y=118  2026-05-20       (date, gray)
```

### Redfish endpoints

```
GET /redfish/v1/                                   Service root
GET /redfish/v1/Chassis/1/Thermal                  Temperature + humidity JSON
GET /redfish/v1/Systems/1/LogServices/SEL/Entries  Last 20 SEL entries
```

Example:

```sh
curl http://<pi-ip>:8080/redfish/v1/Chassis/1/Thermal
```

### SEL log format

```
[2026-06-17 14:03:21] #0001 INFO     SYSTEM        val=   0.0  Monitor started
[2026-06-17 14:05:10] #0002 WARNING  SHT35         val=  30.2  temp=30.2C hum=65.0% (was OK)
[2026-06-17 14:07:44] #0003 INFO     SHT35         val=  29.8  temp=29.8C hum=64.5% (was WARNING)
```

## Raspberry Pi 5 setup

### I2C (SHT35)

1. Enable I2C in `raspi-config`.
2. Confirm the sensor is visible: `i2cdetect -y 1`

### SPI (ST7735)

1. Enable SPI in `raspi-config`.
2. Wire: `VCC→3.3V`, `GND`, `SCL→SCLK`, `SDA→MOSI`, `RES→GPIO25`, `DC→GPIO24`, `CS→CE0`.

### UART (Pico 2)

Add to `/boot/firmware/config.txt`:

```
dtoverlay=uart0-pi5
```

### Backlight PWM (optional)

Add to `/boot/firmware/config.txt`:

```
dtoverlay=pwm,pin=18,func=2
```

## Configuration

Edit `config.ini` before running (or override with environment variables):

```ini
[thresholds]
temp_warning    = 30.0   # IPMI Upper Non-Critical
temp_critical   = 40.0   # IPMI Upper Critical
humidity_warning  = 80.0
humidity_critical = 90.0

[logging]
log_file         = logs/monitor.log
sel_file         = logs/sel.log
console_interval = 20    # seconds between stdout prints

[uart]
uart_device = /dev/ttyAMA0
uart_baud   = 115200

[http]
http_port = 8080
```

### Environment variable overrides

| Variable | Overrides |
|----------|-----------|
| `TEMP_WARNING` | `temp_warning` |
| `TEMP_CRITICAL` | `temp_critical` |
| `HUMIDITY_WARNING` | `humidity_warning` |
| `HUMIDITY_CRITICAL` | `humidity_critical` |
| `PICO_UART_DEV` | `uart_device` |
| `PICO_UART_BAUD` | `uart_baud` |
| `HTTP_PORT` | `http_port` |

## Build and run

```sh
make build
sudo ./pi_lab
```

`sudo` is required for GPIO and SPI access.

### Pico 2 firmware

Copy `pico2/wifi_config.h.example` to `pico2/wifi_config.h` and fill in credentials:

```sh
cp pico2/wifi_config.h.example pico2/wifi_config.h
```

Then build:

```sh
cd pico2
mkdir build && cd build
cmake ..
make
```

Flash `bmc_sim.uf2` to the Pico 2 by holding BOOTSEL and copying to the USB drive.

### Test binaries

```sh
make test_st7735    # hardware display test (requires Pi hardware)
make test_pico2     # UART reader test: make test_pico2 && sudo ./test/test_pico2
```

## Project structure

```
src/
  main.c          — main loop: read → alert → SEL → log → HTTP → display
  config.c        — INI config loader with env-var override
  sel.c           — IPMI-style System Event Log
  http_api.c      — Redfish-inspired HTTP API (pthreads, raw sockets)
  sensor_sht35.c  — SHT35 I2C driver
  sensor_pico.c   — Pico 2 UART reader (BMC frame parser)
  logger.c        — severity-stamped file logger
  alert.c         — dual-threshold alert engine
  st7735.c        — ST7735 SPI driver + 5×7 bitmap font
include/
  config.h
  sel.h
  http_api.h
  sensor_sht35.h
  sensor_pico.h
  logger.h
  alert.h
  st7735.h
pico2/
  bmc_sim.c       — Pico 2 BMC firmware: ADC temp + fan curve + UART TX
  CMakeLists.txt
  wifi_config.h.example
config.ini        — runtime configuration
logs/             — monitor.log + sel.log (git-ignored, created at startup)
scripts/
  sync_pi.sh      — rsync from Mac to Pi
  sync_mac.sh     — rsync from Pi to Mac
```

## Development notes

The project cross-compiles on macOS: `sensor_sht35.c` and `st7735.c` provide non-Linux stubs so `make build` succeeds on both platforms. Use the sync scripts to push changes between machines.

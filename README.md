# Pi Lab

Raspberry Pi hardware interface and sensor management project.

## Hardware target

This project now targets a Sensirion SHT35 temperature/humidity sensor over I2C on Raspberry Pi.

## Raspberry Pi setup

1. Enable I2C in `raspi-config`.
2. Confirm the sensor is visible on bus 1 with `i2cdetect -y 1`.
3. Wire the sensor to `3.3V`, `GND`, `SDA`, and `SCL`.
4. Use the default I2C address `0x44`, or set `SHT35_I2C_ADDR=0x45` if the ADDR pin is pulled high.

## Build and run

```sh
make build
./pi_lab
```

Optional environment variables:

- `SHT35_I2C_BUS` defaults to `1`
- `SHT35_I2C_ADDR` defaults to `0x44`

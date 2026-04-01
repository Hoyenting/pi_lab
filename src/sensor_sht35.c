#include <errno.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include "sensor_sht35.h"

#ifdef __linux__
#include <fcntl.h>
#include <linux/i2c-dev.h>
#include <sys/ioctl.h>

/*
 * Command values and timing come from the Sensirion SHT3x datasheet:
 * - 0x30A2: soft reset
 * - 0x2400: single-shot, high repeatability, clock stretching disabled
 * The 20 ms delay leaves margin above the documented high-repeatability
 * measurement time before reading the six-byte response frame.
 */
#define SHT35_CMD_SOFT_RESET 0x30A2
#define SHT35_CMD_MEASURE_HIGH 0x2400
#define SHT35_MEASUREMENT_DELAY_US 20000

static int sensor_fd = -1;

static int parse_i2c_bus(const char *value, int default_value) {
    char *end = NULL;
    long parsed;

    if (!value || *value == '\0') {
        return default_value;
    }

    parsed = strtol(value, &end, 10);
    if (end == value || *end != '\0' || parsed < 0 || parsed > 255) {
        fprintf(stderr, "Invalid SHT35_I2C_BUS=%s, using default %d\n", value, default_value);
        return default_value;
    }

    return (int)parsed;
}

static int parse_i2c_address(const char *value, int default_value) {
    char *end = NULL;
    long parsed;

    if (!value || *value == '\0') {
        return default_value;
    }

    parsed = strtol(value, &end, 0);
    if (end == value || *end != '\0' || parsed < 0x03 || parsed > 0x77) {
        fprintf(stderr, "Invalid SHT35_I2C_ADDR=%s, using default 0x%02X\n", value, default_value);
        return default_value;
    }

    return (int)parsed;
}

/*
 * CRC-8 parameters come from the SHT3x datasheet:
 * polynomial 0x31, initialization 0xFF, calculated over each two-byte word.
 */
static uint8_t sht35_crc8(const uint8_t *data, size_t len) {
    uint8_t crc = 0xFF;
    size_t i;
    int bit;

    for (i = 0; i < len; ++i) {
        crc ^= data[i];
        for (bit = 0; bit < 8; ++bit) {
            crc = (crc & 0x80) ? (uint8_t)((crc << 1) ^ 0x31) : (uint8_t)(crc << 1);
        }
    }

    return crc;
}

static int write_command(uint16_t command) {
    uint8_t buffer[2];

    buffer[0] = (uint8_t)(command >> 8);
    buffer[1] = (uint8_t)(command & 0xFF);

    if (write(sensor_fd, buffer, sizeof(buffer)) != (ssize_t)sizeof(buffer)) {
        perror("Failed to write SHT35 command");
        return -1;
    }

    return 0;
}
#endif

int sensor_init(void) {
#ifdef __linux__
    char device_path[32];
    const char *bus_env = getenv("SHT35_I2C_BUS");
    const char *addr_env = getenv("SHT35_I2C_ADDR");
    int bus = parse_i2c_bus(bus_env, SHT35_DEFAULT_I2C_BUS);
    int address = parse_i2c_address(addr_env, SHT35_DEFAULT_I2C_ADDR);

    if (sensor_fd >= 0) {
        return 0;
    }

    snprintf(device_path, sizeof(device_path), "/dev/i2c-%d", bus);
    sensor_fd = open(device_path, O_RDWR);
    if (sensor_fd < 0) {
        perror("Failed to open I2C device");
        return -1;
    }

    if (ioctl(sensor_fd, I2C_SLAVE, address) < 0) {
        perror("Failed to set I2C slave address");
        close(sensor_fd);
        sensor_fd = -1;
        return -1;
    }

    if (write_command(SHT35_CMD_SOFT_RESET) != 0) {
        close(sensor_fd);
        sensor_fd = -1;
        return -1;
    }

    usleep(2000);
    return 0;
#else
    errno = ENOSYS;
    fprintf(stderr, "Real SHT35 support requires Linux/Raspberry Pi with i2c-dev\n");
    return -1;
#endif
}

int sensor_read(sensor_data_t *data) {
#ifdef __linux__
    uint8_t raw[6];
    uint16_t raw_temp;
    uint16_t raw_humidity;

    if (!data) {
        errno = EINVAL;
        fprintf(stderr, "sensor_read() received a NULL data pointer\n");
        return -1;
    }

    if (sensor_fd < 0) {
        errno = ENODEV;
        fprintf(stderr, "sensor_read() called before sensor_init()\n");
        return -1;
    }

    if (write_command(SHT35_CMD_MEASURE_HIGH) != 0) {
        fprintf(stderr, "Unable to trigger SHT35 measurement\n");
        return -1;
    }

    usleep(SHT35_MEASUREMENT_DELAY_US);

    if (read(sensor_fd, raw, sizeof(raw)) != (ssize_t)sizeof(raw)) {
        perror("Failed to read SHT35 measurement");
        return -1;
    }

    if (sht35_crc8(raw, 2) != raw[2]) {
        errno = EIO;
        fprintf(stderr, "SHT35 temperature CRC mismatch\n");
        return -1;
    }

    if (sht35_crc8(raw + 3, 2) != raw[5]) {
        errno = EIO;
        fprintf(stderr, "SHT35 humidity CRC mismatch\n");
        return -1;
    }

    raw_temp = (uint16_t)((raw[0] << 8) | raw[1]);
    raw_humidity = (uint16_t)((raw[3] << 8) | raw[4]);

    data->temperature_c = -45.0f + (175.0f * (float)raw_temp / 65535.0f);
    data->humidity_pct = 100.0f * (float)raw_humidity / 65535.0f;
    if (data->humidity_pct < 0.0f) {
        data->humidity_pct = 0.0f;
    } else if (data->humidity_pct > 100.0f) {
        data->humidity_pct = 100.0f;
    }
    return 0;
#else
    (void)data;
    errno = ENOSYS;
    fprintf(stderr, "sensor_read() requires Linux/Raspberry Pi with i2c-dev\n");
    return -1;
#endif
}

void sensor_cleanup(void) {
#ifdef __linux__
    if (sensor_fd >= 0) {
        close(sensor_fd);
        sensor_fd = -1;
    }
#endif
}

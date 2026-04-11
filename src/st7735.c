#define _POSIX_C_SOURCE 200809L

#include "st7735.h"
#include <errno.h>
#include <fcntl.h>
#ifdef __linux__
#include <linux/spi/spidev.h>
#endif
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <time.h>
#include <unistd.h>

#define ST7735_DEFAULT_SPI_DEVICE "/dev/spidev0.0"
#define ST7735_DEFAULT_DC_PIN 24
#define ST7735_DEFAULT_RST_PIN 25
#define ST7735_DEFAULT_BL_PIN 18
#define ST7735_WIDTH 128
#define ST7735_HEIGHT 160
#define ST7735_SPI_SPEED_HZ 8000000

static int spi_fd = -1;
static int dc_pin = -1;
static int rst_pin = -1;
static int bl_pin = -1;

static void sleep_ms(int ms) {
    struct timespec delay = {.tv_sec = ms / 1000, .tv_nsec = (ms % 1000) * 1000000L};
    nanosleep(&delay, NULL);
}

#ifdef __linux__

static int gpio_write_value(int pin, int value) {
    char path[64];
    int fd;

    snprintf(path, sizeof(path), "/sys/class/gpio/gpio%d/value", pin);
    fd = open(path, O_WRONLY);
    if (fd < 0) {
        perror("gpio value open");
        return -1;
    }

    if (write(fd, value ? "1" : "0", 1) != 1) {
        perror("gpio value write");
        close(fd);
        return -1;
    }

    close(fd);
    return 0;
}

static int gpio_set_direction(int pin, const char *direction) {
    char path[64];
    int fd;

    snprintf(path, sizeof(path), "/sys/class/gpio/gpio%d/direction", pin);
    fd = open(path, O_WRONLY);
    if (fd < 0) {
        perror("gpio direction open");
        return -1;
    }

    if (write(fd, direction, strlen(direction)) != (ssize_t)strlen(direction)) {
        perror("gpio direction write");
        close(fd);
        return -1;
    }

    close(fd);
    return 0;
}

static int gpio_export(int pin) {
    char buffer[32];
    int fd;

    fd = open("/sys/class/gpio/export", O_WRONLY);
    if (fd < 0) {
        perror("gpio export open");
        return -1;
    }

    int len = snprintf(buffer, sizeof(buffer), "%d", pin);
    if (write(fd, buffer, len) != len) {
        if (errno != EBUSY) {
            perror("gpio export write");
            close(fd);
            return -1;
        }
    }

    close(fd);
    return 0;
}

static int gpio_unexport(int pin) {
    char buffer[32];
    int fd;

    fd = open("/sys/class/gpio/unexport", O_WRONLY);
    if (fd < 0) {
        perror("gpio unexport open");
        return -1;
    }

    int len = snprintf(buffer, sizeof(buffer), "%d", pin);
    if (write(fd, buffer, len) != len) {
        perror("gpio unexport write");
        close(fd);
        return -1;
    }

    close(fd);
    return 0;
}

static int spi_write(const uint8_t *data, size_t len) {
    ssize_t written = write(spi_fd, data, len);
    if (written != (ssize_t)len) {
        perror("SPI write");
        return -1;
    }
    return 0;
}

static int st7735_write_command(uint8_t command) {
    if (gpio_write_value(dc_pin, 0) != 0) {
        return -1;
    }
    return spi_write(&command, 1);
}

static int st7735_write_data(const uint8_t *data, size_t length) {
    if (gpio_write_value(dc_pin, 1) != 0) {
        return -1;
    }
    return spi_write(data, length);
}

static int st7735_reset(void) {
    if (rst_pin >= 0) {
        if (gpio_write_value(rst_pin, 0) != 0) return -1;
        sleep_ms(50);
        if (gpio_write_value(rst_pin, 1) != 0) return -1;
        sleep_ms(150);
    }
    return 0;
}

static int st7735_set_address_window(uint16_t x0, uint16_t y0, uint16_t x1, uint16_t y1) {
    uint8_t data[4];

    if (st7735_write_command(0x2A) != 0) return -1;
    data[0] = (x0 >> 8) & 0xFF;
    data[1] = x0 & 0xFF;
    data[2] = (x1 >> 8) & 0xFF;
    data[3] = x1 & 0xFF;
    if (st7735_write_data(data, sizeof(data)) != 0) return -1;

    if (st7735_write_command(0x2B) != 0) return -1;
    data[0] = (y0 >> 8) & 0xFF;
    data[1] = y0 & 0xFF;
    data[2] = (y1 >> 8) & 0xFF;
    data[3] = y1 & 0xFF;
    if (st7735_write_data(data, sizeof(data)) != 0) return -1;

    return st7735_write_command(0x2C);
}

static int st7735_init_sequence(void) {
    if (st7735_write_command(0x01) != 0) return -1; // SWRESET
    sleep_ms(150);

    if (st7735_write_command(0x11) != 0) return -1; // SLPOUT
    sleep_ms(500);

    uint8_t data;

    if (st7735_write_command(0x3A) != 0) return -1; // COLMOD
    data = 0x05; // 16-bit color
    if (st7735_write_data(&data, 1) != 0) return -1;

    if (st7735_write_command(0x36) != 0) return -1; // MADCTL
    data = 0xC0;
    if (st7735_write_data(&data, 1) != 0) return -1;

    if (st7735_write_command(0xB1) != 0) return -1;
    data = 0x05;
    if (st7735_write_data(&data, 1) != 0) return -1;
    data = 0x3A;
    if (st7735_write_data(&data, 1) != 0) return -1;
    data = 0x3A;
    if (st7735_write_data(&data, 1) != 0) return -1;

    if (st7735_write_command(0xB2) != 0) return -1;
    data = 0x05;
    if (st7735_write_data(&data, 1) != 0) return -1;
    data = 0x3A;
    if (st7735_write_data(&data, 1) != 0) return -1;
    data = 0x3A;
    if (st7735_write_data(&data, 1) != 0) return -1;

    if (st7735_write_command(0xB3) != 0) return -1;
    data = 0x05;
    if (st7735_write_data(&data, 1) != 0) return -1;
    data = 0x3A;
    if (st7735_write_data(&data, 1) != 0) return -1;
    data = 0x3A;
    if (st7735_write_data(&data, 1) != 0) return -1;
    data = 0x05;
    if (st7735_write_data(&data, 1) != 0) return -1;
    data = 0x3A;
    if (st7735_write_data(&data, 1) != 0) return -1;
    data = 0x3A;
    if (st7735_write_data(&data, 1) != 0) return -1;

    if (st7735_write_command(0xB4) != 0) return -1;
    data = 0x03;
    if (st7735_write_data(&data, 1) != 0) return -1;

    if (st7735_write_command(0xC0) != 0) return -1;
    data = 0x28;
    if (st7735_write_data(&data, 1) != 0) return -1;
    data = 0x08;
    if (st7735_write_data(&data, 1) != 0) return -1;
    data = 0x04;
    if (st7735_write_data(&data, 1) != 0) return -1;

    if (st7735_write_command(0xC1) != 0) return -1;
    data = 0xC0;
    if (st7735_write_data(&data, 1) != 0) return -1;

    if (st7735_write_command(0xC2) != 0) return -1;
    data = 0x0D;
    if (st7735_write_data(&data, 1) != 0) return -1;
    data = 0x00;
    if (st7735_write_data(&data, 1) != 0) return -1;

    if (st7735_write_command(0xC3) != 0) return -1;
    data = 0x8D;
    if (st7735_write_data(&data, 1) != 0) return -1;
    data = 0x2A;
    if (st7735_write_data(&data, 1) != 0) return -1;

    if (st7735_write_command(0xC4) != 0) return -1;
    data = 0x8D;
    if (st7735_write_data(&data, 1) != 0) return -1;
    data = 0xEE;
    if (st7735_write_data(&data, 1) != 0) return -1;

    if (st7735_write_command(0xE0) != 0) return -1;
    uint8_t gamma1[] = {0x04,0x22,0x07,0x0A,0x2E,0x30,0x25,0x2A,0x28,0x26,0x2E,0x3A,0x00,0x01,0x03,0x13};
    if (st7735_write_data(gamma1, sizeof(gamma1)) != 0) return -1;

    if (st7735_write_command(0xE1) != 0) return -1;
    uint8_t gamma2[] = {0x04,0x16,0x06,0x0D,0x2D,0x2E,0x2C,0x2D,0x2E,0x2E,0x37,0x3F,0x00,0x00,0x02,0x10};
    if (st7735_write_data(gamma2, sizeof(gamma2)) != 0) return -1;

    if (st7735_write_command(0x29) != 0) return -1; // DISPON
    sleep_ms(100);

    return 0;
}

static int configure_gpio_pin_from_env(const char *env_name, int default_value) {
    const char *value = getenv(env_name);
    if (!value || *value == '\0') {
        return default_value;
    }
    char *end = NULL;
    long parsed = strtol(value, &end, 10);
    if (end == value || *end != '\0' || parsed < 0) {
        fprintf(stderr, "Invalid %s=%s, using default %d\n", env_name, value, default_value);
        return default_value;
    }
    return (int)parsed;
}

int st7735_init(void) {
    const char *device = getenv("ST7735_SPI_DEV");
    if (!device || *device == '\0') {
        device = ST7735_DEFAULT_SPI_DEVICE;
    }
    dc_pin = configure_gpio_pin_from_env("ST7735_DC_PIN", ST7735_DEFAULT_DC_PIN);
    rst_pin = configure_gpio_pin_from_env("ST7735_RST_PIN", ST7735_DEFAULT_RST_PIN);
    bl_pin = configure_gpio_pin_from_env("ST7735_BL_PIN", ST7735_DEFAULT_BL_PIN);

    if (gpio_export(dc_pin) != 0 || gpio_set_direction(dc_pin, "out") != 0) {
        return -1;
    }
    if (gpio_export(rst_pin) != 0 || gpio_set_direction(rst_pin, "out") != 0) {
        return -1;
    }
    if (gpio_export(bl_pin) != 0 || gpio_set_direction(bl_pin, "out") != 0) {
        return -1;
    }
    if (gpio_write_value(bl_pin, 1) != 0) {
        return -1;
    }

    spi_fd = open(device, O_RDWR);
    if (spi_fd < 0) {
        perror("Failed to open SPI device");
        return -1;
    }

    uint8_t mode = SPI_MODE_0;
    uint8_t bits = 8;
    uint32_t speed = ST7735_SPI_SPEED_HZ;

    if (ioctl(spi_fd, SPI_IOC_WR_MODE, &mode) < 0 || ioctl(spi_fd, SPI_IOC_WR_BITS_PER_WORD, &bits) < 0 || ioctl(spi_fd, SPI_IOC_WR_MAX_SPEED_HZ, &speed) < 0) {
        perror("Failed to configure SPI device");
        close(spi_fd);
        spi_fd = -1;
        return -1;
    }

    if (st7735_reset() != 0) {
        return -1;
    }

    return st7735_init_sequence();
}

int st7735_fill(uint16_t color) {
    if (spi_fd < 0) {
        fprintf(stderr, "st7735_fill() called before init\n");
        return -1;
    }

    if (st7735_set_address_window(0, 0, ST7735_WIDTH - 1, ST7735_HEIGHT - 1) != 0) {
        return -1;
    }

    size_t pixels = ST7735_WIDTH * ST7735_HEIGHT;
    size_t chunk_size = 1024;
    uint8_t buffer[chunk_size * 2];
    for (size_t i = 0; i < chunk_size; i++) {
        buffer[i * 2] = (uint8_t)(color >> 8);
        buffer[i * 2 + 1] = (uint8_t)color;
    }

    while (pixels > 0) {
        size_t this_count = pixels < chunk_size ? pixels : chunk_size;
        if (st7735_write_data(buffer, this_count * 2) != 0) {
            return -1;
        }
        pixels -= this_count;
    }

    return 0;
}

void st7735_cleanup(void) {
    if (spi_fd >= 0) {
        close(spi_fd);
        spi_fd = -1;
    }
    if (bl_pin >= 0) {
        gpio_write_value(bl_pin, 0);
    }
    if (dc_pin >= 0) {
        gpio_unexport(dc_pin);
        dc_pin = -1;
    }
    if (rst_pin >= 0) {
        gpio_unexport(rst_pin);
        rst_pin = -1;
    }
    if (bl_pin >= 0) {
        gpio_unexport(bl_pin);
        bl_pin = -1;
    }
}
#else

int st7735_init(void) {
    errno = ENOSYS;
    fprintf(stderr, "ST7735 support requires Linux/Raspberry Pi with spidev\n");
    return -1;
}

int st7735_fill(uint16_t color) {
    (void)color;
    errno = ENOSYS;
    return -1;
}

void st7735_cleanup(void) {
}
#endif

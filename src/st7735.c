#define _POSIX_C_SOURCE 200809L

#include "st7735.h"
#include <errno.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#ifdef __linux__
#include <fcntl.h>
#include <gpiod.h>
#include <linux/spi/spidev.h>
#include <sys/ioctl.h>
#include <unistd.h>

#define ST7735_DEFAULT_SPI_DEVICE "/dev/spidev0.0"
#define ST7735_DEFAULT_GPIOCHIP "gpiochip0"
#define ST7735_DEFAULT_DC_LINE 24
#define ST7735_DEFAULT_RST_LINE 25
#define ST7735_WIDTH 128
#define ST7735_HEIGHT 160
#define ST7735_SPI_SPEED_HZ 8000000

static int spi_fd = -1;
static struct gpiod_chip *chip = NULL;
static struct gpiod_line *dc_line = NULL;
static struct gpiod_line *rst_line = NULL;

static void sleep_ms(int ms) {
    struct timespec delay = {.tv_sec = ms / 1000, .tv_nsec = (ms % 1000) * 1000000L};
    nanosleep(&delay, NULL);
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
    if (gpiod_line_set_value(dc_line, 0) != 0) {
        perror("gpiod_line_set_value (DC)");
        return -1;
    }
    return spi_write(&command, 1);
}

static int st7735_write_data(const uint8_t *data, size_t length) {
    if (gpiod_line_set_value(dc_line, 1) != 0) {
        perror("gpiod_line_set_value (DC)");
        return -1;
    }
    return spi_write(data, length);
}

static int st7735_reset(void) {
    if (gpiod_line_set_value(rst_line, 0) != 0) {
        perror("gpiod_line_set_value (RST low)");
        return -1;
    }
    sleep_ms(50);
    if (gpiod_line_set_value(rst_line, 1) != 0) {
        perror("gpiod_line_set_value (RST high)");
        return -1;
    }
    sleep_ms(150);
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

static int configure_gpio_line_from_env(const char *env_name, int default_value) {
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

    const char *gpiochip = getenv("ST7735_GPIOCHIP");
    if (!gpiochip || *gpiochip == '\0') {
        gpiochip = ST7735_DEFAULT_GPIOCHIP;
    }

    int dc_offset = configure_gpio_line_from_env("ST7735_DC_LINE", ST7735_DEFAULT_DC_LINE);
    int rst_offset = configure_gpio_line_from_env("ST7735_RST_LINE", ST7735_DEFAULT_RST_LINE);

    /* Open GPIO chip */
    chip = gpiod_chip_open_by_name(gpiochip);
    if (!chip) {
        perror("gpiod_chip_open_by_name");
        return -1;
    }

    /* Request DC line as output */
    dc_line = gpiod_chip_get_line(chip, dc_offset);
    if (!dc_line) {
        fprintf(stderr, "Failed to get DC line %d\n", dc_offset);
        gpiod_chip_close(chip);
        chip = NULL;
        return -1;
    }

    if (gpiod_line_request_output(dc_line, "st7735_dc", 0) != 0) {
        perror("gpiod_line_request_output (DC)");
        gpiod_chip_close(chip);
        chip = NULL;
        dc_line = NULL;
        return -1;
    }

    /* Request RST line as output */
    rst_line = gpiod_chip_get_line(chip, rst_offset);
    if (!rst_line) {
        fprintf(stderr, "Failed to get RST line %d\n", rst_offset);
        gpiod_line_release(dc_line);
        gpiod_chip_close(chip);
        chip = NULL;
        dc_line = NULL;
        return -1;
    }

    if (gpiod_line_request_output(rst_line, "st7735_rst", 1) != 0) {
        perror("gpiod_line_request_output (RST)");
        gpiod_line_release(dc_line);
        gpiod_chip_close(chip);
        chip = NULL;
        dc_line = NULL;
        rst_line = NULL;
        return -1;
    }

    /* Open SPI device */
    spi_fd = open(device, O_RDWR);
    if (spi_fd < 0) {
        perror("Failed to open SPI device");
        gpiod_line_release(rst_line);
        gpiod_line_release(dc_line);
        gpiod_chip_close(chip);
        chip = NULL;
        rst_line = NULL;
        dc_line = NULL;
        return -1;
    }

    /* Configure SPI device */
    uint8_t mode = SPI_MODE_0;
    uint8_t bits = 8;
    uint32_t speed = ST7735_SPI_SPEED_HZ;

    if (ioctl(spi_fd, SPI_IOC_WR_MODE, &mode) < 0 || 
        ioctl(spi_fd, SPI_IOC_WR_BITS_PER_WORD, &bits) < 0 || 
        ioctl(spi_fd, SPI_IOC_WR_MAX_SPEED_HZ, &speed) < 0) {
        perror("Failed to configure SPI device");
        close(spi_fd);
        spi_fd = -1;
        gpiod_line_release(rst_line);
        gpiod_line_release(dc_line);
        gpiod_chip_close(chip);
        chip = NULL;
        rst_line = NULL;
        dc_line = NULL;
        return -1;
    }

    /* Reset and initialize display */
    if (st7735_reset() != 0) {
        close(spi_fd);
        spi_fd = -1;
        gpiod_line_release(rst_line);
        gpiod_line_release(dc_line);
        gpiod_chip_close(chip);
        chip = NULL;
        rst_line = NULL;
        dc_line = NULL;
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
    if (dc_line) {
        gpiod_line_release(dc_line);
        dc_line = NULL;
    }
    if (rst_line) {
        gpiod_line_release(rst_line);
        rst_line = NULL;
    }
    if (chip) {
        gpiod_chip_close(chip);
        chip = NULL;
    }
}
#else

int st7735_init(void) {
    errno = ENOSYS;
    fprintf(stderr, "ST7735 support requires Linux/Raspberry Pi with libgpiod and spidev\n");
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

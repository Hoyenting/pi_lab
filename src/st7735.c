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
#define ST7735_DEFAULT_DC_PIN 24
#define ST7735_DEFAULT_RST_PIN 25
#define ST7735_WIDTH 128
#define ST7735_HEIGHT 160
#define ST7735_SPI_SPEED_HZ 8000000
#define GPIO_CONSUMER "st7735_driver"

#define ST7735_PWM_CHIP_DEFAULT "/sys/class/pwm/pwmchip0"
#define ST7735_PWM_CHANNEL_DEFAULT 2   /* GPIO18 = PWM0_CHAN2 (ALT3) on RP1 */
#define ST7735_PWM_PERIOD_NS 1000000  /* 1 kHz */
#define ST7735_PWM_BL_PIN_DEFAULT 18
#define ST7735_PWM_BL_ALT_DEFAULT "a3"

static int spi_fd = -1;
static struct gpiod_chip *gpio_chip = NULL;
static struct gpiod_line_request *line_request = NULL;
static int pwm_enabled = 0;
static char pwm_path_base[160]; /* e.g. /sys/class/pwm/pwmchip0/pwm0 */

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
    gpiod_line_request_set_value(line_request, ST7735_DEFAULT_DC_PIN, GPIOD_LINE_VALUE_INACTIVE);
    return spi_write(&command, 1);
}

static int st7735_write_data(const uint8_t *data, size_t length) {
    gpiod_line_request_set_value(line_request, ST7735_DEFAULT_DC_PIN, GPIOD_LINE_VALUE_ACTIVE);
    return spi_write(data, length);
}

static int st7735_reset(void) {
    gpiod_line_request_set_value(line_request, ST7735_DEFAULT_RST_PIN, GPIOD_LINE_VALUE_INACTIVE);
    sleep_ms(50);
    gpiod_line_request_set_value(line_request, ST7735_DEFAULT_RST_PIN, GPIOD_LINE_VALUE_ACTIVE);
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

static int pwm_write_file(const char *path, const char *value) {
    int fd = open(path, O_WRONLY);
    if (fd < 0) return -1;
    ssize_t len = (ssize_t)strlen(value);
    ssize_t n = write(fd, value, len);
    close(fd);
    return (n == len) ? 0 : -1;
}

int st7735_backlight_init(void) {
    const char *chip = getenv("ST7735_PWM_CHIP");
    if (!chip || *chip == '\0') chip = ST7735_PWM_CHIP_DEFAULT;

    /* Ensure GPIO is in PWM alt-function mode.
     * gpioset/libgpiod will silently switch the pad back to GPIO mode;
     * this restores it so sysfs PWM actually drives the pin. */
    {
        const char *bl_alt = getenv("ST7735_BL_ALT");
        if (!bl_alt || *bl_alt == '\0') bl_alt = ST7735_PWM_BL_ALT_DEFAULT;
        int bl_pin = ST7735_PWM_BL_PIN_DEFAULT;
        const char *bl_pin_env = getenv("ST7735_BL_PIN");
        if (bl_pin_env && *bl_pin_env != '\0') {
            char *end = NULL;
            long p = strtol(bl_pin_env, &end, 10);
            if (end != bl_pin_env && *end == '\0' && p >= 0) bl_pin = (int)p;
        }
        char cmd[64];
        snprintf(cmd, sizeof(cmd), "pinctrl set %d %s", bl_pin, bl_alt);
        if (system(cmd) != 0)
            fprintf(stderr, "Warning: '%s' failed — PWM may not drive backlight\n", cmd);
    }

    char path[200];
    char val[32];

    /* Export channel (ignore error if already exported) */
    snprintf(path, sizeof(path), "%s/export", chip);
    snprintf(val, sizeof(val), "%d", ST7735_PWM_CHANNEL_DEFAULT);
    pwm_write_file(path, val);
    sleep_ms(100);

    snprintf(pwm_path_base, sizeof(pwm_path_base), "%s/pwm%d", chip, ST7735_PWM_CHANNEL_DEFAULT);

    /* Set period */
    snprintf(path, sizeof(path), "%s/period", pwm_path_base);
    snprintf(val, sizeof(val), "%d", ST7735_PWM_PERIOD_NS);
    if (pwm_write_file(path, val) != 0) {
        perror("PWM set period");
        return -1;
    }

    /* Full brightness as initial duty cycle */
    snprintf(path, sizeof(path), "%s/duty_cycle", pwm_path_base);
    snprintf(val, sizeof(val), "%d", ST7735_PWM_PERIOD_NS);
    if (pwm_write_file(path, val) != 0) {
        perror("PWM set duty_cycle");
        return -1;
    }

    /* Enable */
    snprintf(path, sizeof(path), "%s/enable", pwm_path_base);
    if (pwm_write_file(path, "1") != 0) {
        perror("PWM enable");
        return -1;
    }

    pwm_enabled = 1;
    return 0;
}

int st7735_set_backlight(uint8_t percent) {
    if (!pwm_enabled) return -1;
    if (percent > 100) percent = 100;

    char path[200];
    char val[32];
    long duty = (long)ST7735_PWM_PERIOD_NS * percent / 100;

    snprintf(path, sizeof(path), "%s/duty_cycle", pwm_path_base);
    snprintf(val, sizeof(val), "%ld", duty);
    if (pwm_write_file(path, val) != 0) {
        perror("PWM set duty_cycle");
        return -1;
    }
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

    int dc_pin_num = configure_gpio_pin_from_env("ST7735_DC_PIN", ST7735_DEFAULT_DC_PIN);
    int rst_pin_num = configure_gpio_pin_from_env("ST7735_RST_PIN", ST7735_DEFAULT_RST_PIN);

    /* Open GPIO chip (try gpiochip4 first for RPi 5, fallback to gpiochip0) */
    gpio_chip = gpiod_chip_open("/dev/gpiochip4");
    if (!gpio_chip) {
        gpio_chip = gpiod_chip_open("/dev/gpiochip0");
    }

    if (!gpio_chip) {
        perror("gpiod_chip_open");
        return -1;
    }

    /* Create line config */
    struct gpiod_line_config *line_cfg = gpiod_line_config_new();
    if (!line_cfg) {
        perror("gpiod_line_config_new");
        gpiod_chip_close(gpio_chip);
        gpio_chip = NULL;
        return -1;
    }

    /* Set DC line as output, initially inactive (LOW) */
    struct gpiod_line_settings *dc_settings = gpiod_line_settings_new();
    if (!dc_settings) {
        perror("gpiod_line_settings_new for DC");
        gpiod_line_config_free(line_cfg);
        gpiod_chip_close(gpio_chip);
        gpio_chip = NULL;
        return -1;
    }
    gpiod_line_settings_set_direction(dc_settings, GPIOD_LINE_DIRECTION_OUTPUT);
    gpiod_line_settings_set_output_value(dc_settings, GPIOD_LINE_VALUE_INACTIVE);
    unsigned int dc_lines[] = {dc_pin_num};
    gpiod_line_config_add_line_settings(line_cfg, dc_lines, 1, dc_settings);
    gpiod_line_settings_free(dc_settings);

    /* Set RST line as output, initially active (HIGH) */
    struct gpiod_line_settings *rst_settings = gpiod_line_settings_new();
    if (!rst_settings) {
        perror("gpiod_line_settings_new for RST");
        gpiod_line_config_free(line_cfg);
        gpiod_chip_close(gpio_chip);
        gpio_chip = NULL;
        return -1;
    }
    gpiod_line_settings_set_direction(rst_settings, GPIOD_LINE_DIRECTION_OUTPUT);
    gpiod_line_settings_set_output_value(rst_settings, GPIOD_LINE_VALUE_ACTIVE);
    unsigned int rst_lines[] = {rst_pin_num};
    gpiod_line_config_add_line_settings(line_cfg, rst_lines, 1, rst_settings);
    gpiod_line_settings_free(rst_settings);

    /* Create request config */
    struct gpiod_request_config *req_cfg = gpiod_request_config_new();
    if (!req_cfg) {
        perror("gpiod_request_config_new");
        gpiod_line_config_free(line_cfg);
        gpiod_chip_close(gpio_chip);
        gpio_chip = NULL;
        return -1;
    }
    gpiod_request_config_set_consumer(req_cfg, GPIO_CONSUMER);

    /* Request lines */
    line_request = gpiod_chip_request_lines(gpio_chip, req_cfg, line_cfg);
    if (!line_request) {
        perror("gpiod_chip_request_lines");
        gpiod_request_config_free(req_cfg);
        gpiod_line_config_free(line_cfg);
        gpiod_chip_close(gpio_chip);
        gpio_chip = NULL;
        return -1;
    }

    gpiod_request_config_free(req_cfg);
    gpiod_line_config_free(line_cfg);

    /* Open SPI device */
    spi_fd = open(device, O_RDWR);
    if (spi_fd < 0) {
        perror("Failed to open SPI device");
        gpiod_line_request_release(line_request);
        line_request = NULL;
        gpiod_chip_close(gpio_chip);
        gpio_chip = NULL;
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
        gpiod_line_request_release(line_request);
        line_request = NULL;
        gpiod_chip_close(gpio_chip);
        gpio_chip = NULL;
        return -1;
    }

    /* Reset and initialize display */
    if (st7735_reset() != 0) {
        close(spi_fd);
        spi_fd = -1;
        gpiod_line_request_release(line_request);
        line_request = NULL;
        gpiod_chip_close(gpio_chip);
        gpio_chip = NULL;
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
    if (pwm_enabled) {
        char path[200];
        snprintf(path, sizeof(path), "%s/enable", pwm_path_base);
        pwm_write_file(path, "0");

        const char *chip = getenv("ST7735_PWM_CHIP");
        if (!chip || *chip == '\0') chip = ST7735_PWM_CHIP_DEFAULT;
        snprintf(path, sizeof(path), "%s/unexport", chip);
        char val[32];
        snprintf(val, sizeof(val), "%d", ST7735_PWM_CHANNEL_DEFAULT);
        pwm_write_file(path, val);
        pwm_enabled = 0;
    }
    if (line_request) {
        gpiod_line_request_release(line_request);
        line_request = NULL;
    }
    if (gpio_chip) {
        gpiod_chip_close(gpio_chip);
        gpio_chip = NULL;
    }
    if (spi_fd >= 0) {
        close(spi_fd);
        spi_fd = -1;
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

int st7735_backlight_init(void) {
    errno = ENOSYS;
    return -1;
}

int st7735_set_backlight(uint8_t percent) {
    (void)percent;
    errno = ENOSYS;
    return -1;
}

void st7735_cleanup(void) {
}

#endif

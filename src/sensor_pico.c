#define _DEFAULT_SOURCE

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "sensor_pico.h"

#ifdef __linux__
#include <fcntl.h>
#include <termios.h>
#include <unistd.h>

static int uart_fd = -1;

static speed_t baud_to_speed(int baud) {
    switch (baud) {
        case 9600:   return B9600;
        case 19200:  return B19200;
        case 38400:  return B38400;
        case 57600:  return B57600;
        case 115200: return B115200;
        default:     return B115200;
    }
}

/*
 * Read one '\n'-terminated line from uart_fd into buf.
 * VTIME=10 gives a 1 s timeout per read(1) call; returns -1 with
 * errno=ETIMEDOUT if no byte arrives within that window.
 */
static int read_line(char *buf, size_t size) {
    size_t i = 0;

    while (i < size - 1) {
        char c;
        ssize_t n = read(uart_fd, &c, 1);
        if (n < 0) {
            perror("UART read");
            return -1;
        }
        if (n == 0) {
            errno = ETIMEDOUT;
            fprintf(stderr, "pico_sensor_read(): UART read timed out\n");
            return -1;
        }
        if (c == '\n') {
            buf[i] = '\0';
            return 0;
        }
        buf[i++] = c;
    }

    buf[i] = '\0';
    errno = ENOBUFS;
    return -1;
}
#endif /* __linux__ */

int pico_sensor_init(const char *device, int baud) {
#ifdef __linux__
    if (uart_fd >= 0)
        return 0;

    if (!device || *device == '\0')
        device = getenv("PICO_UART_DEV");
    if (!device || *device == '\0')
        device = PICO_DEFAULT_UART_DEV;

    if (baud <= 0) {
        const char *env = getenv("PICO_UART_BAUD");
        baud = (env && *env) ? atoi(env) : PICO_DEFAULT_BAUD;
    }

    uart_fd = open(device, O_RDWR | O_NOCTTY | O_SYNC);
    if (uart_fd < 0) {
        perror("Failed to open UART device");
        return -1;
    }

    struct termios tty;
    if (tcgetattr(uart_fd, &tty) != 0) {
        perror("tcgetattr");
        close(uart_fd);
        uart_fd = -1;
        return -1;
    }

    speed_t speed = baud_to_speed(baud);
    cfsetospeed(&tty, speed);
    cfsetispeed(&tty, speed);

    cfmakeraw(&tty);        /* 8N1, no echo, no signal chars */
    tty.c_cc[VMIN]  = 0;   /* return immediately if no bytes */
    tty.c_cc[VTIME] = 10;  /* inter-char timeout: 10 × 100 ms = 1 s */

    if (tcsetattr(uart_fd, TCSANOW, &tty) != 0) {
        perror("tcsetattr");
        close(uart_fd);
        uart_fd = -1;
        return -1;
    }

    return 0;
#else
    (void)device; (void)baud;
    errno = ENOSYS;
    fprintf(stderr, "pico_sensor_init() requires Linux\n");
    return -1;
#endif
}

int pico_sensor_read(pico_data_t *data) {
#ifdef __linux__
    if (!data) {
        errno = EINVAL;
        return -1;
    }
    if (uart_fd < 0) {
        errno = ENODEV;
        fprintf(stderr, "pico_sensor_read() called before pico_sensor_init()\n");
        return -1;
    }

    char line[64];
    if (read_line(line, sizeof(line)) != 0)
        return -1;

    /* Extended format: BMC:TEMP:XX.X,FAN:XXXX */
    if (sscanf(line, "BMC:TEMP:%f,FAN:%d", &data->cpu_temp_c, &data->fan_rpm) == 2)
        return 0;

    /* Legacy format: TEMP:XX.X */
    if (sscanf(line, "TEMP:%f", &data->cpu_temp_c) == 1) {
        data->fan_rpm = 0;
        return 0;
    }

    errno = EBADMSG;
    fprintf(stderr, "pico_sensor_read(): unexpected line: \"%s\"\n", line);
    return -1;
#else
    (void)data;
    errno = ENOSYS;
    return -1;
#endif
}

void pico_sensor_cleanup(void) {
#ifdef __linux__
    if (uart_fd >= 0) {
        close(uart_fd);
        uart_fd = -1;
    }
#endif
}

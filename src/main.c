#include <stdio.h>
#include <stdlib.h>
#include <signal.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include "sensor_sht35.h"
#include "sensor_pico.h"
#include "logger.h"
#include "alert.h"
#include "st7735.h"

#define LOG_FILE "logs/monitor.log"

#define COLOR_BLACK  0x0000u
#define COLOR_WHITE  0xFFFFu
#define COLOR_GRAY   0x8410u
#define COLOR_CYAN   0x07FFu
#define COLOR_GREEN  0x07E0u
#define COLOR_RED    0xF800u
#define COLOR_YELLOW 0xFFE0u

static volatile sig_atomic_t keep_running = 1;

static void handle_signal(int signo) {
    (void)signo;
    keep_running = 0;
}

/*
 * Screen layout (128×160, black background):
 *   y=  2  "SHT35"       scale 1, gray
 *   y= 12  " 25.3C"      scale 2, white   (SHT35 temperature)
 *   y= 28  " 62.1%"      scale 2, cyan    (SHT35 humidity)
 *   y= 50  "PICO2 CPU"   scale 1, gray
 *   y= 60  " 45.2C"      scale 2, yellow  (Pico 2 CPU temp, or "  ---" if no data)
 *   y= 82  "OK    "      scale 2, green/red
 *   y=104  "17:28:47"    scale 1, gray
 *   y=114  "2026-05-20"  scale 1, gray
 */
static void display_update(const sensor_data_t *data, const char *status,
                            float pico_temp, int pico_valid,
                            const char *time_str, const char *date_str) {
    char buf[16];

    st7735_draw_string(10,   2, "SHT35",    COLOR_GRAY,   COLOR_BLACK, 1);

    snprintf(buf, sizeof(buf), "%5.1fC", data->temperature_c);
    st7735_draw_string(10,  12, buf,         COLOR_WHITE,  COLOR_BLACK, 2);

    snprintf(buf, sizeof(buf), "%5.1f%%", data->humidity_pct);
    st7735_draw_string(10,  28, buf,         COLOR_CYAN,   COLOR_BLACK, 2);

    st7735_draw_string(10,  50, "PICO2 CPU", COLOR_GRAY,   COLOR_BLACK, 1);

    if (pico_valid)
        snprintf(buf, sizeof(buf), "%5.1fC", pico_temp);
    else
        snprintf(buf, sizeof(buf), "  ---");
    st7735_draw_string(10,  60, buf,         COLOR_YELLOW, COLOR_BLACK, 2);

    snprintf(buf, sizeof(buf), "%-6s", status);
    uint16_t sc = (strcmp(status, "OK") == 0) ? COLOR_GREEN : COLOR_RED;
    st7735_draw_string(10,  82, buf,         sc,           COLOR_BLACK, 2);

    st7735_draw_string(10, 104, time_str,    COLOR_GRAY,   COLOR_BLACK, 1);
    st7735_draw_string(10, 114, date_str,    COLOR_GRAY,   COLOR_BLACK, 1);
}

int main(void) {
    sensor_data_t data;
    char timestamp[20];
    char date_str[12];
    char time_str[10];
    time_t now;
    struct tm *tm_info;
    float pico_temp = 0.0f;
    int   pico_valid = 0;
    int   pico_ready = 0;
    time_t last_print = 0;

    signal(SIGINT,  handle_signal);
    signal(SIGTERM, handle_signal);

    if (sensor_init() != 0) {
        fprintf(stderr, "Failed to initialize sensor\n");
        return 1;
    }

    if (logger_init(LOG_FILE) != 0) {
        fprintf(stderr, "Failed to initialize logger\n");
        sensor_cleanup();
        return 1;
    }

    if (st7735_init() != 0) {
        fprintf(stderr, "Failed to initialize display\n");
        sensor_cleanup();
        logger_close();
        return 1;
    }
    st7735_backlight_init();
    st7735_fill(COLOR_BLACK);

    /* Pico 2 UART — non-fatal if unavailable */
    if (pico_sensor_init(NULL, 0) == 0) {
        pico_ready = 1;
        printf("Pico 2 UART ready\n");
    } else {
        fprintf(stderr, "Pico 2 UART unavailable — showing --- for CPU temp\n");
    }

    printf("Starting environment monitor...\n");

    while (keep_running) {
        time(&now);
        tm_info = localtime(&now);
        strftime(timestamp, sizeof(timestamp), "%Y-%m-%d %H:%M:%S", tm_info);
        strftime(date_str,  sizeof(date_str),  "%Y-%m-%d",          tm_info);
        strftime(time_str,  sizeof(time_str),  "%H:%M:%S",          tm_info);

        if (sensor_read(&data) != 0) {
            fprintf(stderr, "Failed to read SHT35 data\n");
        }

        /* Block-read Pico UART (up to 1 s timeout); keeps last value on failure */
        if (pico_ready) {
            float t;
            if (pico_sensor_read(&t) == 0) {
                pico_temp  = t;
                pico_valid = 1;
            } else {
                pico_valid = 0;
            }
        }

        const char *status = alert_check(data);

        if (now - last_print >= 20) {
            printf("[%s] sht35=%.1fC/%.1f%% pico=%s%.1fC status=%s\n",
                   timestamp,
                   data.temperature_c, data.humidity_pct,
                   pico_valid ? "" : "(stale)",
                   pico_temp,
                   status);
            last_print = now;
        }

        logger_write(timestamp, data, status);

        display_update(&data, status, pico_temp, pico_valid, time_str, date_str);
    }

    sensor_cleanup();
    if (pico_ready) pico_sensor_cleanup();
    logger_close();
    st7735_fill(COLOR_BLACK);
    st7735_cleanup();
    printf("Environment monitor stopped.\n");

    return 0;
}

#include <stdio.h>
#include <stdlib.h>
#include <signal.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include "sensor_sht35.h"
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

static volatile sig_atomic_t keep_running = 1;

static void handle_signal(int signo) {
    (void)signo;
    keep_running = 0;
}

/*
 * Screen layout (128×160, black background):
 *   y=  8  "Temp:"       scale 2, gray
 *   y= 26  " 25.3C"      scale 3, white   (6 chars fixed-width)
 *   y= 55  "Hum:"        scale 2, gray
 *   y= 73  " 62.1%"      scale 3, cyan    (6 chars fixed-width)
 *   y=102  "OK    "      scale 2, green/red (6 chars fixed-width)
 *   y=124  "17:28:47"    scale 1, gray
 *   y=134  "2026-05-20"  scale 1, gray
 */
static void display_update(const sensor_data_t *data, const char *status,
                            const char *time_str, const char *date_str) {
    char buf[16];

    st7735_draw_string(10,   8, "Temp:", COLOR_GRAY,  COLOR_BLACK, 2);

    snprintf(buf, sizeof(buf), "%5.1fC", data->temperature_c);
    st7735_draw_string(10,  26, buf,     COLOR_WHITE, COLOR_BLACK, 3);

    st7735_draw_string(10,  55, "Hum:",  COLOR_GRAY,  COLOR_BLACK, 2);

    snprintf(buf, sizeof(buf), "%5.1f%%", data->humidity_pct);
    st7735_draw_string(10,  73, buf,     COLOR_CYAN,  COLOR_BLACK, 3);

    snprintf(buf, sizeof(buf), "%-6s", status);
    uint16_t sc = (strcmp(status, "OK") == 0) ? COLOR_GREEN : COLOR_RED;
    st7735_draw_string(10, 102, buf,     sc,          COLOR_BLACK, 2);

    st7735_draw_string(10, 124, time_str, COLOR_GRAY, COLOR_BLACK, 1);
    st7735_draw_string(10, 134, date_str, COLOR_GRAY, COLOR_BLACK, 1);
}

int main(void) {
    sensor_data_t data;
    char timestamp[20];
    char date_str[12];
    char time_str[10];
    time_t now;
    struct tm *tm_info;

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

    printf("Starting environment monitor...\n");

    while (keep_running) {
        time(&now);
        tm_info = localtime(&now);
        strftime(timestamp, sizeof(timestamp), "%Y-%m-%d %H:%M:%S", tm_info);
        strftime(date_str,  sizeof(date_str),  "%Y-%m-%d",          tm_info);
        strftime(time_str,  sizeof(time_str),  "%H:%M:%S",          tm_info);

        if (sensor_read(&data) != 0) {
            fprintf(stderr, "Failed to read sensor data\n");
            continue;
        }

        const char *status = alert_check(data);

        printf("[%s] temp=%.1fC humidity=%.1f%% status=%s\n",
               timestamp, data.temperature_c, data.humidity_pct, status);

        logger_write(timestamp, data, status);

        display_update(&data, status, time_str, date_str);

        sleep(1);
    }

    sensor_cleanup();
    logger_close();
    st7735_fill(COLOR_BLACK);
    st7735_cleanup();
    printf("Environment monitor stopped.\n");

    return 0;
}

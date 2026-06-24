#include <stdio.h>
#include <stdlib.h>
#include <signal.h>
#include <string.h>
#include <sys/stat.h>
#include <time.h>
#include <unistd.h>
#include "config.h"
#include "sensor_sht35.h"
#include "sensor_pico.h"
#include "logger.h"
#include "alert.h"
#include "sel.h"
#include "http_api.h"
#include "st7735.h"

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
 *   y= 48  "PICO2 CPU"   scale 1, gray
 *   y= 58  " 45.2C"      scale 2, yellow  (Pico 2 CPU temp, or "  ---")
 *   y= 76  "FAN 2400RPM" scale 1, gray    (fan RPM, or "FAN ---" if no data)
 *   y= 86  "OK    "      scale 2, green / "WARN  " yellow / "CRIT  " red
 *   y=108  "17:28:47"    scale 1, gray
 *   y=118  "2026-05-20"  scale 1, gray
 */
static void display_update(const sensor_data_t *data, alert_level_t level,
                            float pico_temp, int fan_rpm, int pico_valid,
                            const char *time_str, const char *date_str) {
    char buf[20];

    st7735_draw_string(10,   2, "SHT35",    COLOR_GRAY,   COLOR_BLACK, 1);

    snprintf(buf, sizeof(buf), "%5.1fC", data->temperature_c);
    st7735_draw_string(10,  12, buf,         COLOR_WHITE,  COLOR_BLACK, 2);

    snprintf(buf, sizeof(buf), "%5.1f%%", data->humidity_pct);
    st7735_draw_string(10,  28, buf,         COLOR_CYAN,   COLOR_BLACK, 2);

    st7735_draw_string(10,  48, "PICO2 CPU", COLOR_GRAY,   COLOR_BLACK, 1);

    if (pico_valid)
        snprintf(buf, sizeof(buf), "%5.1fC", pico_temp);
    else
        snprintf(buf, sizeof(buf), "  ---");
    st7735_draw_string(10,  58, buf,         COLOR_YELLOW, COLOR_BLACK, 2);

    if (pico_valid && fan_rpm > 0)
        snprintf(buf, sizeof(buf), "FAN%5dRPM", fan_rpm);
    else
        snprintf(buf, sizeof(buf), "FAN    ---");
    st7735_draw_string(10,  76, buf,         COLOR_GRAY,   COLOR_BLACK, 1);

    const char *label;
    uint16_t    label_color;
    switch (level) {
        case ALERT_CRITICAL: label = "CRIT  "; label_color = COLOR_RED;    break;
        case ALERT_WARNING:  label = "WARN  "; label_color = COLOR_YELLOW; break;
        default:             label = "OK    "; label_color = COLOR_GREEN;  break;
    }
    st7735_draw_string(10,  86, label,       label_color,  COLOR_BLACK, 2);

    st7735_draw_string(10, 108, time_str,    COLOR_GRAY,   COLOR_BLACK, 1);
    st7735_draw_string(10, 118, date_str,    COLOR_GRAY,   COLOR_BLACK, 1);
}

int main(void) {
    pi_lab_config_t cfg;
    sensor_data_t   data;
    char  timestamp[20];
    char  date_str[12];
    char  time_str[10];
    time_t now;
    struct tm *tm_info;
    pico_data_t pico     = {0};
    int         pico_valid = 0;
    int         pico_ready = 0;
    time_t last_print = 0;
    alert_level_t prev_level = ALERT_OK;

    signal(SIGINT,  handle_signal);
    signal(SIGTERM, handle_signal);

    config_load(&cfg, NULL);
    config_print(&cfg);

    /* Ensure log directory exists */
    mkdir("logs", 0755);

    if (sensor_init() != 0) {
        fprintf(stderr, "Failed to initialize sensor\n");
        return 1;
    }

    if (logger_init(cfg.log_file) != 0) {
        fprintf(stderr, "Failed to initialize logger\n");
        sensor_cleanup();
        return 1;
    }

    if (sel_init(cfg.sel_file) != 0)
        fprintf(stderr, "SEL init failed — event log unavailable\n");

    if (st7735_init() != 0) {
        fprintf(stderr, "Failed to initialize display\n");
        sensor_cleanup();
        logger_close();
        sel_close();
        return 1;
    }
    st7735_backlight_init();
    st7735_fill(COLOR_BLACK);

    /* Pico 2 UART — non-fatal if unavailable */
    if (pico_sensor_init(cfg.uart_device, cfg.uart_baud) == 0) {
        pico_ready = 1;
        printf("Pico 2 UART ready on %s @ %d\n", cfg.uart_device, cfg.uart_baud);
    } else {
        fprintf(stderr, "Pico 2 UART unavailable — showing --- for CPU temp\n");
    }

    http_api_start(cfg.http_port);

    sel_add(SEL_INFO, "SYSTEM", 0.0f, "Monitor started");
    printf("Starting environment monitor...\n");

    while (keep_running) {
        time(&now);
        tm_info = localtime(&now);
        strftime(timestamp, sizeof(timestamp), "%Y-%m-%d %H:%M:%S", tm_info);
        strftime(date_str,  sizeof(date_str),  "%Y-%m-%d",          tm_info);
        strftime(time_str,  sizeof(time_str),  "%H:%M:%S",          tm_info);

        if (sensor_read(&data) != 0)
            fprintf(stderr, "Failed to read SHT35 data\n");

        if (pico_ready) {
            pico_data_t d;
            if (pico_sensor_read(&d) == 0) {
                pico       = d;
                pico_valid = 1;
            } else {
                pico_valid = 0;
            }
        }

        alert_level_t level = alert_check(data, &cfg);

        /* Write SEL entry only on state transition */
        if (level != prev_level) {
            char desc[64];
            snprintf(desc, sizeof(desc), "temp=%.1fC hum=%.1f%% (was %s)",
                     data.temperature_c, data.humidity_pct,
                     alert_level_str(prev_level));
            sel_severity_t sev = (level == ALERT_CRITICAL) ? SEL_CRITICAL :
                                 (level == ALERT_WARNING)  ? SEL_WARNING  : SEL_INFO;
            sel_add(sev, "SHT35", data.temperature_c, desc);
            prev_level = level;
        }

        if (now - last_print >= cfg.console_interval_s) {
            printf("[%s] sht35=%.1fC/%.1f%% pico=%s%.1fC fan=%dRPM status=%s\n",
                   timestamp,
                   data.temperature_c, data.humidity_pct,
                   pico_valid ? "" : "(stale)",
                   pico.cpu_temp_c, pico.fan_rpm,
                   alert_level_str(level));
            last_print = now;
        }

        logger_write(timestamp, data, pico.cpu_temp_c, pico_valid, level);

        http_sensor_snapshot_t snap = {
            .temp_c       = data.temperature_c,
            .humidity_pct = data.humidity_pct,
            .pico_temp_c  = pico.cpu_temp_c,
            .pico_valid   = pico_valid,
            .level        = level,
        };
        http_api_update(&snap);

        display_update(&data, level, pico.cpu_temp_c, pico.fan_rpm,
                       pico_valid, time_str, date_str);
    }

    sel_add(SEL_INFO, "SYSTEM", 0.0f, "Monitor stopped");
    http_api_stop();
    sensor_cleanup();
    if (pico_ready) pico_sensor_cleanup();
    logger_close();
    sel_close();
    st7735_fill(COLOR_BLACK);
    st7735_cleanup();
    printf("Environment monitor stopped.\n");

    return 0;
}

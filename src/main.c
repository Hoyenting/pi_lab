#include <stdio.h>
#include <stdlib.h>
#include <signal.h>
#include <time.h>
#include <unistd.h>
#include "sensor_sht35.h"
#include "logger.h"
#include "alert.h"

#define LOG_FILE "logs/monitor.log"

static volatile sig_atomic_t keep_running = 1;

static void handle_signal(int signo) {
    (void)signo;
    keep_running = 0;
}

int main() {
    sensor_data_t data;
    char timestamp[20];
    time_t now;
    struct tm *tm_info;

    signal(SIGINT, handle_signal);
    signal(SIGTERM, handle_signal);

    // Initialize sensor
    if (sensor_init() != 0) {
        fprintf(stderr, "Failed to initialize sensor\n");
        return 1;
    }

    // Initialize logger
    if (logger_init(LOG_FILE) != 0) {
        fprintf(stderr, "Failed to initialize logger\n");
        return 1;
    }

    printf("Starting environment monitor...\n");

    // Main monitoring loop
    while (keep_running) {
        // Get current time
        time(&now);
        tm_info = localtime(&now);
        strftime(timestamp, sizeof(timestamp), "%Y-%m-%d %H:%M:%S", tm_info);

        // Read sensor data
        if (sensor_read(&data) != 0) {
            fprintf(stderr, "Failed to read sensor data\n");
            continue;
        }

        // Check alert status
        const char *status = alert_check(data);

        // Print to console
        printf("[%s] temp=%.1fC humidity=%.1f%% status=%s\n",
               timestamp, data.temperature_c, data.humidity_pct, status);

        // Write to log
        logger_write(timestamp, data, status);

        // Wait 1 second
        sleep(1);
    }

    sensor_cleanup();
    logger_close();
    printf("Environment monitor stopped.\n");

    return 0;
}

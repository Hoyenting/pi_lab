#include <stdio.h>
#include "logger.h"

static FILE *log_fp = NULL;

static const char *log_level_str(alert_level_t level) {
    switch (level) {
        case ALERT_CRITICAL: return "CRITICAL";
        case ALERT_WARNING:  return "WARNING";
        default:             return "INFO";
    }
}

int logger_init(const char *log_file) {
    log_fp = fopen(log_file, "a");
    if (!log_fp) {
        perror("Failed to open log file");
        return -1;
    }
    return 0;
}

int logger_write(const char *timestamp, sensor_data_t data,
                 float pico_temp, int pico_valid, alert_level_t level) {
    if (!log_fp) return -1;

    char pico_buf[12];
    if (pico_valid)
        snprintf(pico_buf, sizeof(pico_buf), "%.1fC", pico_temp);
    else
        snprintf(pico_buf, sizeof(pico_buf), "stale");

    fprintf(log_fp, "[%s] %-8s temp=%5.1fC humidity=%5.1f%% pico_cpu=%s status=%s\n",
            timestamp, log_level_str(level),
            data.temperature_c, data.humidity_pct,
            pico_buf, alert_level_str(level));
    fflush(log_fp);
    return 0;
}

void logger_close(void) {
    if (log_fp) {
        fclose(log_fp);
        log_fp = NULL;
    }
}

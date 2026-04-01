#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "logger.h"

static FILE *log_fp = NULL;

int logger_init(const char *log_file) {
    log_fp = fopen(log_file, "a");
    if (!log_fp) {
        perror("Failed to open log file");
        return -1;
    }
    return 0;
}

int logger_write(const char *timestamp, sensor_data_t data, const char *status) {
    if (!log_fp) return -1;

    fprintf(log_fp, "[%s] temp=%.1fC humidity=%.1f%% status=%s\n",
            timestamp, data.temperature_c, data.humidity_pct, status);
    fflush(log_fp);  // Ensure immediate write
    return 0;
}

void logger_close(void) {
    if (log_fp) {
        fclose(log_fp);
        log_fp = NULL;
    }
}

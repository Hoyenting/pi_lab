#include <stdio.h>
#include <time.h>
#include "sel.h"

static FILE    *sel_fp  = NULL;
static uint32_t next_id = 1;

int sel_init(const char *path) {
    sel_fp = fopen(path, "a");
    if (!sel_fp) {
        perror("sel_init: fopen");
        return -1;
    }
    return 0;
}

const char *sel_severity_str(sel_severity_t s) {
    switch (s) {
        case SEL_INFO:     return "INFO";
        case SEL_WARNING:  return "WARNING";
        case SEL_CRITICAL: return "CRITICAL";
        default:           return "UNKNOWN";
    }
}

void sel_add(sel_severity_t severity, const char *sensor,
             float value, const char *description) {
    if (!sel_fp) return;

    char ts[20];
    time_t now = time(NULL);
    struct tm *t = localtime(&now);
    strftime(ts, sizeof(ts), "%Y-%m-%d %H:%M:%S", t);

    fprintf(sel_fp, "[%s] #%04u %-8s %-12s val=%6.1f  %s\n",
            ts, next_id++, sel_severity_str(severity), sensor, value, description);
    fflush(sel_fp);
}

void sel_close(void) {
    if (sel_fp) {
        fclose(sel_fp);
        sel_fp = NULL;
    }
}

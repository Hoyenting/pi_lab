#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "config.h"

static void config_defaults(pi_lab_config_t *cfg) {
    cfg->temp_warning       = 30.0f;
    cfg->temp_critical      = 40.0f;
    cfg->humidity_warning   = 80.0f;
    cfg->humidity_critical  = 90.0f;
    snprintf(cfg->log_file,    sizeof(cfg->log_file),    "logs/monitor.log");
    snprintf(cfg->sel_file,    sizeof(cfg->sel_file),    "logs/sel.log");
    cfg->console_interval_s = 20;
    snprintf(cfg->uart_device, sizeof(cfg->uart_device), "/dev/ttyAMA0");
    cfg->uart_baud          = 115200;
    cfg->http_port          = 8080;
}

static char *trim(char *s) {
    while (isspace((unsigned char)*s)) s++;
    char *end = s + strlen(s);
    while (end > s && isspace((unsigned char)*(end - 1))) end--;
    *end = '\0';
    return s;
}

int config_load(pi_lab_config_t *cfg, const char *path) {
    config_defaults(cfg);
    if (!path || !*path) path = CONFIG_DEFAULT_PATH;

    FILE *f = fopen(path, "r");
    if (f) {
        char line[256];
        while (fgets(line, sizeof(line), f)) {
            char *s = trim(line);
            if (*s == '#' || *s == ';' || *s == '[' || *s == '\0') continue;
            char *eq = strchr(s, '=');
            if (!eq) continue;
            *eq = '\0';
            char *key = trim(s);
            char *val = trim(eq + 1);

            if      (!strcmp(key, "temp_warning"))       cfg->temp_warning       = strtof(val, NULL);
            else if (!strcmp(key, "temp_critical"))      cfg->temp_critical      = strtof(val, NULL);
            else if (!strcmp(key, "humidity_warning"))   cfg->humidity_warning   = strtof(val, NULL);
            else if (!strcmp(key, "humidity_critical"))  cfg->humidity_critical  = strtof(val, NULL);
            else if (!strcmp(key, "log_file"))           snprintf(cfg->log_file,    sizeof(cfg->log_file),    "%s", val);
            else if (!strcmp(key, "sel_file"))           snprintf(cfg->sel_file,    sizeof(cfg->sel_file),    "%s", val);
            else if (!strcmp(key, "console_interval"))   cfg->console_interval_s = atoi(val);
            else if (!strcmp(key, "uart_device"))        snprintf(cfg->uart_device, sizeof(cfg->uart_device), "%s", val);
            else if (!strcmp(key, "uart_baud"))          cfg->uart_baud          = atoi(val);
            else if (!strcmp(key, "http_port"))          cfg->http_port          = atoi(val);
        }
        fclose(f);
    }

    /* Environment variables override config file values */
    const char *e;
    if ((e = getenv("TEMP_WARNING"))      && *e) cfg->temp_warning       = strtof(e, NULL);
    if ((e = getenv("TEMP_CRITICAL"))     && *e) cfg->temp_critical      = strtof(e, NULL);
    if ((e = getenv("HUMIDITY_WARNING"))  && *e) cfg->humidity_warning   = strtof(e, NULL);
    if ((e = getenv("HUMIDITY_CRITICAL")) && *e) cfg->humidity_critical  = strtof(e, NULL);
    if ((e = getenv("LOG_FILE"))          && *e) snprintf(cfg->log_file,    sizeof(cfg->log_file),    "%s", e);
    if ((e = getenv("SEL_FILE"))          && *e) snprintf(cfg->sel_file,    sizeof(cfg->sel_file),    "%s", e);
    if ((e = getenv("CONSOLE_INTERVAL"))  && *e) cfg->console_interval_s = atoi(e);
    if ((e = getenv("PICO_UART_DEV"))     && *e) snprintf(cfg->uart_device, sizeof(cfg->uart_device), "%s", e);
    if ((e = getenv("PICO_UART_BAUD"))    && *e) cfg->uart_baud          = atoi(e);
    if ((e = getenv("HTTP_PORT"))         && *e) cfg->http_port          = atoi(e);

    return 0;
}

void config_print(const pi_lab_config_t *cfg) {
    printf("[config] temp  warn=%.1fC crit=%.1fC  "
           "humidity warn=%.1f%% crit=%.1f%%\n",
           cfg->temp_warning, cfg->temp_critical,
           cfg->humidity_warning, cfg->humidity_critical);
    printf("[config] log=%s  sel=%s  interval=%ds\n",
           cfg->log_file, cfg->sel_file, cfg->console_interval_s);
    printf("[config] uart=%s @ %d baud\n", cfg->uart_device, cfg->uart_baud);
    printf("[config] http port=%d\n", cfg->http_port);
}

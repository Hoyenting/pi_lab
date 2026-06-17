#ifndef CONFIG_H
#define CONFIG_H

#define CONFIG_DEFAULT_PATH "config.ini"

typedef struct {
    /* Temperature thresholds (°C) */
    float temp_warning;
    float temp_critical;
    /* Humidity thresholds (%) */
    float humidity_warning;
    float humidity_critical;
    /* Logging */
    char log_file[256];
    char sel_file[256];
    int  console_interval_s;
    /* UART */
    char uart_device[64];
    int  uart_baud;
    /* HTTP API */
    int  http_port;
} pi_lab_config_t;

/* Load from file; env vars override file values. Safe defaults if file absent. */
int  config_load(pi_lab_config_t *cfg, const char *path);
void config_print(const pi_lab_config_t *cfg);

#endif /* CONFIG_H */

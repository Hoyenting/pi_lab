#ifndef ALERT_H
#define ALERT_H

#include "sensor_sht35.h"
#include "config.h"

typedef enum {
    ALERT_OK       = 0,
    ALERT_WARNING  = 1,
    ALERT_CRITICAL = 2,
} alert_level_t;

alert_level_t alert_check(sensor_data_t data, const pi_lab_config_t *cfg);
const char   *alert_level_str(alert_level_t level);

#endif /* ALERT_H */

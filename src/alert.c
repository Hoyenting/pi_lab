#include "alert.h"

const char *alert_level_str(alert_level_t level) {
    switch (level) {
        case ALERT_OK:       return "OK";
        case ALERT_WARNING:  return "WARNING";
        case ALERT_CRITICAL: return "CRITICAL";
        default:             return "UNKNOWN";
    }
}

alert_level_t alert_check(sensor_data_t data, const pi_lab_config_t *cfg) {
    if (data.temperature_c >= cfg->temp_critical ||
        data.humidity_pct  >= cfg->humidity_critical)
        return ALERT_CRITICAL;

    if (data.temperature_c >= cfg->temp_warning ||
        data.humidity_pct  >= cfg->humidity_warning)
        return ALERT_WARNING;

    return ALERT_OK;
}

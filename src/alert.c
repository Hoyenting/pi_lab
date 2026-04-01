#include <string.h>
#include "alert.h"

const char* alert_check(sensor_data_t data) {
    // Simple alert logic: ALERT if temperature > 30C
    if (data.temperature_c > 30.0) {
        return "ALERT";
    }
    return "OK";
}

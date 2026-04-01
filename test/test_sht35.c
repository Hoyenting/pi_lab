#include <stdio.h>
#include "../include/sensor_sht35.h"

int main() {
    sensor_data_t data;

    if (sensor_init() != 0) {
        printf("Sensor initialization failed\n");
        return 1;
    }

    if (sensor_read(&data) != 0) {
        printf("Sensor read failed\n");
        sensor_cleanup();
        return 1;
    }

    printf("Temperature: %.2f °C\n", data.temperature_c);
    printf("Humidity: %.2f %%\n", data.humidity_pct);

    sensor_cleanup();
    return 0;
}
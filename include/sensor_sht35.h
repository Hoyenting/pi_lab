#ifndef SENSOR_SHT35_H
#define SENSOR_SHT35_H

#include <stdint.h>

#define SHT35_DEFAULT_I2C_BUS 1
#define SHT35_DEFAULT_I2C_ADDR 0x44

typedef struct {
    float temperature_c;
    float humidity_pct;
} sensor_data_t;

int sensor_init(void);
int sensor_read(sensor_data_t *data);
void sensor_cleanup(void);

#endif // SENSOR_SHT35_H

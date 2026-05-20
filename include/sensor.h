#ifndef SENSOR_H
#define SENSOR_H

typedef struct {
    float temperature_c;
    float humidity_pct;
} sensor_data_t;

int sensor_init(void);
int sensor_read(sensor_data_t *data);
void sensor_cleanup(void);

#endif
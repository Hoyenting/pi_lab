#ifndef LOGGER_H
#define LOGGER_H

#include "sensor_sht35.h"
#include "alert.h"

int  logger_init(const char *log_file);
int  logger_write(const char *timestamp, sensor_data_t data,
                  float pico_temp, int pico_valid, alert_level_t level);
void logger_close(void);

#endif /* LOGGER_H */

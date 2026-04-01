#ifndef LOGGER_H
#define LOGGER_H

#include "sensor_sht35.h"

int logger_init(const char *log_file);
int logger_write(const char *timestamp, sensor_data_t data, const char *status);
void logger_close(void);

#endif // LOGGER_H

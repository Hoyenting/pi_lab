#ifndef SENSOR_PICO_H
#define SENSOR_PICO_H

#define PICO_DEFAULT_UART_DEV "/dev/ttyAMA10"
#define PICO_DEFAULT_BAUD     115200

/* Pass NULL/0 to use env vars PICO_UART_DEV / PICO_UART_BAUD or defaults. */
int   pico_sensor_init(const char *device, int baud);
int   pico_sensor_read(float *temp_c);
void  pico_sensor_cleanup(void);

#endif /* SENSOR_PICO_H */

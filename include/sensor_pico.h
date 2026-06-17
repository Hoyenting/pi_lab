#ifndef SENSOR_PICO_H
#define SENSOR_PICO_H

#define PICO_DEFAULT_UART_DEV "/dev/ttyAMA0"
#define PICO_DEFAULT_BAUD     115200

typedef struct {
    float cpu_temp_c;
    int   fan_rpm;
} pico_data_t;

/* Pass NULL/0 to use env vars PICO_UART_DEV / PICO_UART_BAUD or defaults. */
int   pico_sensor_init(const char *device, int baud);

/*
 * Read one BMC frame from UART.  Accepts both the extended format
 * "BMC:TEMP:XX.X,FAN:XXXX\n" and the legacy "TEMP:XX.X\n".
 * fan_rpm is set to 0 when using the legacy format.
 */
int   pico_sensor_read(pico_data_t *data);
void  pico_sensor_cleanup(void);

#endif /* SENSOR_PICO_H */

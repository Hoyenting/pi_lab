#include <stdio.h>
#include <stdlib.h>
#include "../include/sensor_pico.h"

#define READ_COUNT 5

int main(int argc, char *argv[]) {
    const char *dev = (argc > 1) ? argv[1] : NULL;

    if (pico_sensor_init(dev, 0) != 0) {
        fprintf(stderr, "UART init failed\n");
        return 1;
    }

    printf("Reading %d samples from Pico 2 (device: %s)\n",
           READ_COUNT, dev ? dev : PICO_DEFAULT_UART_DEV);

    int ok = 0;
    for (int i = 0; i < READ_COUNT; i++) {
        float temp;
        if (pico_sensor_read(&temp) != 0) {
            fprintf(stderr, "Read %d failed\n", i + 1);
        } else {
            printf("[%d] CPU temp: %.1f °C\n", i + 1, temp);
            ok++;
        }
    }

    pico_sensor_cleanup();

    if (ok == 0) {
        fprintf(stderr, "All reads failed\n");
        return 1;
    }

    printf("%d/%d reads succeeded\n", ok, READ_COUNT);
    return 0;
}

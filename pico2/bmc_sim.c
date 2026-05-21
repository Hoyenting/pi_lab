#include <stdio.h>
#include "pico/stdlib.h"
#include "hardware/uart.h"

#define UART_ID      uart0
#define BAUD_RATE    115200
#define UART_TX_PIN  0
#define UART_RX_PIN  1

#define TEMP_IDLE    45.0f
#define SEND_MS      1000

/* 32-bit LCG — good enough for temperature noise without pulling in stdlib rand */
static uint32_t lcg_state = 0xdeadbeef;

static float lcg_noise(float range) {
    lcg_state = lcg_state * 1664525u + 1013904223u;
    float f = (float)(lcg_state >> 1) / (float)0x7fffffffu;
    return (f - 0.5f) * 2.0f * range;
}

/*
 * Simple thermal model: decays toward idle baseline with occasional
 * load spikes every ~30 s to mimic a CPU workload burst.
 */
static float next_temp(void) {
    static float temp = TEMP_IDLE;
    static uint32_t tick = 0;

    tick++;
    if (tick % 30 == 0)
        temp += 12.0f;                          /* load spike */
    else
        temp += (TEMP_IDLE - temp) * 0.08f + lcg_noise(0.4f);  /* decay + jitter */

    if (temp < 30.0f) temp = 30.0f;
    if (temp > 95.0f) temp = 95.0f;

    return temp;
}

int main(void) {
    stdio_init_all();   /* USB CDC for debug; UART0 is for data only */

    uart_init(UART_ID, BAUD_RATE);
    gpio_set_function(UART_TX_PIN, GPIO_FUNC_UART);
    gpio_set_function(UART_RX_PIN, GPIO_FUNC_UART);

    char buf[32];

    while (true) {
        float t = next_temp();
        int n = snprintf(buf, sizeof(buf), "TEMP:%.1f\n", t);
        uart_write_blocking(UART_ID, (const uint8_t *)buf, (size_t)n);

        printf("[bmc_sim] sent: %s", buf);   /* USB CDC debug */
        sleep_ms(SEND_MS);
    }
}

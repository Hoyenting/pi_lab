#include <stdio.h>
#include "pico/stdlib.h"
#include "pico/cyw43_arch.h"
#include "hardware/uart.h"
#include "hardware/adc.h"
#include "hardware/pwm.h"
#include "lwip/netif.h"
#include "wifi_config.h"

#define UART_ID     uart0
#define BAUD_RATE   115200
#define UART_TX_PIN 0
#define UART_RX_PIN 1

/* Fan PWM output — simulates a tachometer signal on GP2 */
#define FAN_PWM_PIN 2

#define SEND_MS     1000

/* Minimum and maximum fan speed (RPM) */
#define FAN_RPM_MIN  800
#define FAN_RPM_MAX  4000

/* Temperature range that drives the fan curve (°C) */
#define FAN_TEMP_LOW  30.0f
#define FAN_TEMP_HIGH 70.0f

static float read_chip_temp(void) {
    adc_select_input(4);
    uint16_t raw = adc_read();
    float voltage = raw * (3.3f / (1 << 12));
    return 27.0f - (voltage - 0.706f) / 0.001721f;
}

/*
 * Linear fan curve: FAN_RPM_MIN at FAN_TEMP_LOW, FAN_RPM_MAX at FAN_TEMP_HIGH.
 * Clamps at both ends.
 */
static int fan_rpm_for_temp(float temp_c) {
    if (temp_c <= FAN_TEMP_LOW)  return FAN_RPM_MIN;
    if (temp_c >= FAN_TEMP_HIGH) return FAN_RPM_MAX;
    float ratio = (temp_c - FAN_TEMP_LOW) / (FAN_TEMP_HIGH - FAN_TEMP_LOW);
    return FAN_RPM_MIN + (int)(ratio * (FAN_RPM_MAX - FAN_RPM_MIN));
}

static void fan_pwm_set(int rpm) {
    /* Drive GP2 with a PWM frequency proportional to RPM for open-loop control.
     * Real tachometers emit 2 pulses/rev; here we approximate for simulation. */
    uint slice = pwm_gpio_to_slice_num(FAN_PWM_PIN);
    uint chan  = pwm_gpio_to_channel(FAN_PWM_PIN);

    if (rpm <= 0) {
        pwm_set_chan_level(slice, chan, 0);
        return;
    }

    /* Target frequency: rpm / 60 * 2 pulses */
    uint32_t freq_hz   = (uint32_t)rpm / 60 * 2;
    uint32_t clk_hz    = 125000000;
    uint32_t wrap      = 999;
    float    divider   = (float)clk_hz / ((float)freq_hz * (wrap + 1));
    if (divider < 1.0f) divider = 1.0f;

    pwm_set_clkdiv(slice, divider);
    pwm_set_wrap(slice, wrap);
    pwm_set_chan_level(slice, chan, wrap / 2); /* 50% duty cycle */
    pwm_set_enabled(slice, true);
}

int main(void) {
    stdio_init_all();

    adc_init();
    adc_set_temp_sensor_enabled(true);

    uart_init(UART_ID, BAUD_RATE);
    gpio_set_function(UART_TX_PIN, GPIO_FUNC_UART);
    gpio_set_function(UART_RX_PIN, GPIO_FUNC_UART);

    gpio_set_function(FAN_PWM_PIN, GPIO_FUNC_PWM);

    if (cyw43_arch_init()) {
        printf("[bmc] WiFi init failed\n");
    } else {
        cyw43_arch_enable_sta_mode();
        printf("[bmc] Connecting to %s...\n", WIFI_SSID);
        if (cyw43_arch_wifi_connect_timeout_ms(WIFI_SSID, WIFI_PASS,
                                               CYW43_AUTH_WPA2_AES_PSK, 15000)) {
            printf("[bmc] WiFi connect failed\n");
        } else {
            printf("[bmc] Connected, IP: %s\n",
                   ip4addr_ntoa(netif_ip4_addr(netif_list)));
        }
    }

    char buf[48];
    while (true) {
        float cpu_temp = read_chip_temp();
        int   fan_rpm  = fan_rpm_for_temp(cpu_temp);

        fan_pwm_set(fan_rpm);

        int n = snprintf(buf, sizeof(buf),
                         "BMC:TEMP:%.1f,FAN:%d\n", cpu_temp, fan_rpm);
        uart_write_blocking(UART_ID, (const uint8_t *)buf, (size_t)n);
        printf("[bmc] %s", buf);

        sleep_ms(SEND_MS);
    }
}

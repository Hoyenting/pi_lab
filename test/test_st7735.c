#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include "../include/st7735.h"

static uint16_t rgb565(uint8_t r, uint8_t g, uint8_t b) {
    return (uint16_t)(((r & 0xF8) << 8) | ((g & 0xFC) << 3) | (b >> 3));
}

static int test_colors(void) {
    printf("--- Color test ---\n");

    const struct { const char *name; uint16_t color; } cases[] = {
        {"red",   0},
        {"green", 0},
        {"blue",  0},
        {"white", 0},
        {"black", 0},
    };
    uint16_t colors[] = {
        rgb565(255,   0,   0),
        rgb565(  0, 255,   0),
        rgb565(  0,   0, 255),
        rgb565(255, 255, 255),
        rgb565(  0,   0,   0),
    };

    for (int i = 0; i < 5; i++) {
        printf("  %s\n", cases[i].name);
        if (st7735_fill(colors[i]) != 0) {
            fprintf(stderr, "Failed to draw %s screen\n", cases[i].name);
            return -1;
        }
        sleep(1);
    }
    return 0;
}

static int test_backlight(void) {
    printf("--- Backlight test (GPIO 18 / PWM0) ---\n");
    printf("  Requires: dtoverlay=pwm,pin=18,func=3 in /boot/firmware/config.txt\n");

    if (st7735_backlight_init() != 0) {
        fprintf(stderr, "  SKIP: backlight init failed (PWM not available)\n");
        return 0;
    }

    /* White background makes brightness change clearly visible */
    if (st7735_fill(rgb565(255, 255, 255)) != 0) {
        fprintf(stderr, "  Failed to fill white for backlight test\n");
        return -1;
    }

    const uint8_t levels[] = {100, 75, 50, 25, 0, 25, 50, 75, 100};
    for (int i = 0; i < (int)(sizeof(levels) / sizeof(levels[0])); i++) {
        printf("  brightness %3d%%\n", levels[i]);
        if (st7735_set_backlight(levels[i]) != 0) {
            fprintf(stderr, "  Failed to set backlight to %d%%\n", levels[i]);
            return -1;
        }
        sleep(1);
    }

    printf("  Backlight test complete.\n");
    return 0;
}

int main(void) {
    if (st7735_init() != 0) {
        fprintf(stderr, "Failed to initialize ST7735 display\n");
        return 1;
    }
    printf("ST7735 initialized.\n");

    if (st7735_backlight_init() == 0) {
        st7735_set_backlight(100);
    }

    if (test_colors() != 0) goto cleanup;
    if (test_backlight() != 0) goto cleanup;

    printf("All tests passed.\n");

cleanup:
    st7735_cleanup();
    return 0;
}

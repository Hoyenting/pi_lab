#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include "../include/st7735.h"

static uint16_t rgb565(uint8_t r, uint8_t g, uint8_t b) {
    return (uint16_t)(((r & 0xF8) << 8) | ((g & 0xFC) << 3) | (b >> 3));
}

int main(void) {
    if (st7735_init() != 0) {
        fprintf(stderr, "Failed to initialize ST7735 display\n");
        return 1;
    }

    printf("ST7735 initialized. Showing test colors...\n");

    if (st7735_fill(rgb565(255, 0, 0)) != 0) {
        fprintf(stderr, "Failed to draw red screen\n");
        goto cleanup;
    }
    sleep(1);

    if (st7735_fill(rgb565(0, 255, 0)) != 0) {
        fprintf(stderr, "Failed to draw green screen\n");
        goto cleanup;
    }
    sleep(1);

    if (st7735_fill(rgb565(0, 0, 255)) != 0) {
        fprintf(stderr, "Failed to draw blue screen\n");
        goto cleanup;
    }
    sleep(1);

    if (st7735_fill(rgb565(255, 255, 255)) != 0) {
        fprintf(stderr, "Failed to draw white screen\n");
        goto cleanup;
    }
    sleep(1);

    if (st7735_fill(rgb565(0, 0, 0)) != 0) {
        fprintf(stderr, "Failed to draw black screen\n");
        goto cleanup;
    }
    sleep(1);

    printf("ST7735 test complete. If the display cycled colors, it is working.\n");

cleanup:
    st7735_cleanup();
    return 0;
}

#ifndef ST7735_H
#define ST7735_H

#include <stdint.h>

int st7735_init(void);
int st7735_fill(uint16_t color);
void st7735_cleanup(void);

/* Backlight control via hardware PWM (GPIO 18 / pwmchip0).
 * Requires dtoverlay=pwm,pin=18,func=2 in /boot/firmware/config.txt.
 * Call after st7735_init(). */
int st7735_backlight_init(void);
int st7735_set_backlight(uint8_t percent); /* 0–100 */

/* Text rendering.  scale 1–4; fg/bg are RGB565 colors. */
void st7735_draw_string(uint16_t x, uint16_t y, const char *str,
                        uint16_t fg, uint16_t bg, uint8_t scale);

#endif // ST7735_H

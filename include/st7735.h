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

#endif // ST7735_H

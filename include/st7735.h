#ifndef ST7735_H
#define ST7735_H

#include <stdint.h>

int st7735_init(void);
int st7735_fill(uint16_t color);
void st7735_cleanup(void);

#endif // ST7735_H

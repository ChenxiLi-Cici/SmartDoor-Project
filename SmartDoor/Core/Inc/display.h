#ifndef DISPLAY_H
#define DISPLAY_H

#include "main.h"

// Function Prototypes
void lcd_print(const char *line1, const char *line2);
void lcd_cursor_at(uint8_t row, uint8_t col);
void lcd_cursor_hide(void);

#endif

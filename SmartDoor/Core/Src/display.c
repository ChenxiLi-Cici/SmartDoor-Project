#include "display.h"
#include "lcd.h"

#define LCD_COLS 16

// print line1 and line2 to the LCD
void lcd_print(const char *line1, const char *line2) {
	const char *lines[2] = { line1, line2 };

	for (int i = 0; i < 2; i++) {
		const char *text = lines[i];
		uint8_t written = 0;

		// Move the cursor to the beginning of the line and then output
		LCD_SetCursor(i, 0);

		while (text != 0 && *text != '\0' && written < LCD_COLS) {
			LCD_SendData((uint8_t)(*text));
			text++;
			written++;
		}
		// Fill in Spaces at the end
		while (written < LCD_COLS) {
			LCD_SendData(' ');
			written++;
		}
	}

}

// Show the blinking hardware cursor at the given position.
void lcd_cursor_at(uint8_t row, uint8_t col)
{
	LCD_SetCursor(row, col);
	LCD_CursorOn();
}

// hide the cursor
void lcd_cursor_hide(void)
{
	LCD_CursorOff();
}

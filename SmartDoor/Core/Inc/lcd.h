#ifndef LCD_H
#define LCD_H

#include "main.h"

#define LCD_D4_PORT   GPIOC
#define LCD_D4_PIN    GPIO_PIN_8
#define LCD_D5_PORT   GPIOC
#define LCD_D5_PIN    GPIO_PIN_9
#define LCD_D6_PORT   GPIOC
#define LCD_D6_PIN    GPIO_PIN_10
#define LCD_D7_PORT   GPIOC
#define LCD_D7_PIN    GPIO_PIN_11

#define LCD_E_PORT    GPIOD
#define LCD_E_PIN     GPIO_PIN_2

#define LCD_RW_PORT   GPIOC
#define LCD_RW_PIN    GPIO_PIN_6

#define LCD_RS_PORT   GPIOA
#define LCD_RS_PIN    GPIO_PIN_15

#define LCD_CLEAR_DISPLAY   0b00000001
#define LCD_RETURN_HOME     0b00000010
#define LCD_ENTRY_MODE      0b00000110
#define LCD_DISPLAY_OFF     0b00001000
#define LCD_DISPLAY_ON      0b00001100
#define LCD_DISPLAY_ON_CURSOR 0b00001111
#define LCD_FUNCTION_SET    0b00101000

#define LCD_ROW1_START      0x00
#define LCD_ROW2_START      0x40

void LCD_Init(void);
void LCD_Pulse(void);
void LCD_PutNibble(uint8_t nibble);
void LCD_SendCmd(uint8_t c);
void LCD_SendData(uint8_t c);
void LCD_SendStr(char *str);
void LCD_SetCursor(uint8_t row, uint8_t col);
void LCD_Clear(void);
void LCD_CursorOn(void);
void LCD_CursorOff(void);

#endif /* LCD_H */

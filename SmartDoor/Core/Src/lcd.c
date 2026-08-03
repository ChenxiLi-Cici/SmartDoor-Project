#include "lcd.h"

static void LCD_DelayUs(uint32_t us)
{
    uint32_t cycles = us * (SystemCoreClock / 1000000) / 5;
    for (uint32_t i = 0; i < cycles; i++)
    {
        __NOP();
    }
}

void LCD_Pulse(void)
{
    HAL_GPIO_WritePin(LCD_E_PORT, LCD_E_PIN, GPIO_PIN_SET);
    LCD_DelayUs(2);
    HAL_GPIO_WritePin(LCD_E_PORT, LCD_E_PIN, GPIO_PIN_RESET);
    LCD_DelayUs(50);
}

void LCD_PutNibble(uint8_t nibble)
{
    HAL_GPIO_WritePin(LCD_D4_PORT, LCD_D4_PIN,
                      (nibble & 0x01) ? GPIO_PIN_SET : GPIO_PIN_RESET);
    HAL_GPIO_WritePin(LCD_D5_PORT, LCD_D5_PIN,
                      (nibble & 0x02) ? GPIO_PIN_SET : GPIO_PIN_RESET);
    HAL_GPIO_WritePin(LCD_D6_PORT, LCD_D6_PIN,
                      (nibble & 0x04) ? GPIO_PIN_SET : GPIO_PIN_RESET);
    HAL_GPIO_WritePin(LCD_D7_PORT, LCD_D7_PIN,
                      (nibble & 0x08) ? GPIO_PIN_SET : GPIO_PIN_RESET);

    LCD_Pulse();
}

void LCD_SendCmd(uint8_t c)
{
    HAL_GPIO_WritePin(LCD_RS_PORT, LCD_RS_PIN, GPIO_PIN_RESET);
    HAL_GPIO_WritePin(LCD_RW_PORT, LCD_RW_PIN, GPIO_PIN_RESET);

    LCD_PutNibble(c >> 4);
    LCD_PutNibble(c & 0x0F);

    HAL_Delay(2);
}

void LCD_SendData(uint8_t c)
{
    HAL_GPIO_WritePin(LCD_RS_PORT, LCD_RS_PIN, GPIO_PIN_SET);
    HAL_GPIO_WritePin(LCD_RW_PORT, LCD_RW_PIN, GPIO_PIN_RESET);

    LCD_PutNibble(c >> 4);
    LCD_PutNibble(c & 0x0F);

    LCD_DelayUs(50);
}

void LCD_SendStr(char *str)
{
    while (*str)
    {
        LCD_SendData((uint8_t)(*str));
        str++;
    }
}

void LCD_SetCursor(uint8_t row, uint8_t col)
{
    uint8_t addr = (row == 0) ? (LCD_ROW1_START + col)
                               : (LCD_ROW2_START + col);
    LCD_SendCmd(0x80 | addr);
}

void LCD_Clear(void)
{
    LCD_SendCmd(LCD_CLEAR_DISPLAY);
    HAL_Delay(2);
}

void LCD_CursorOn(void)
{
    LCD_SendCmd(LCD_DISPLAY_ON_CURSOR);
}

void LCD_CursorOff(void)
{
    LCD_SendCmd(LCD_DISPLAY_ON);
}

void LCD_Init(void)
{
    HAL_GPIO_WritePin(LCD_E_PORT,  LCD_E_PIN,  GPIO_PIN_RESET);
    HAL_GPIO_WritePin(LCD_RS_PORT, LCD_RS_PIN, GPIO_PIN_RESET);
    HAL_GPIO_WritePin(LCD_RW_PORT, LCD_RW_PIN, GPIO_PIN_RESET);

    HAL_Delay(50);

    LCD_PutNibble(0b0011);
    HAL_Delay(5);

    LCD_PutNibble(0b0011);
    HAL_Delay(1);

    LCD_PutNibble(0b0011);
    HAL_Delay(1);

    LCD_PutNibble(0b0010);
    HAL_Delay(1);

    LCD_SendCmd(LCD_FUNCTION_SET);

    LCD_SendCmd(LCD_DISPLAY_OFF);

    LCD_SendCmd(LCD_CLEAR_DISPLAY);
    HAL_Delay(2);

    LCD_SendCmd(LCD_ENTRY_MODE);

    LCD_SendCmd(LCD_DISPLAY_ON);
}

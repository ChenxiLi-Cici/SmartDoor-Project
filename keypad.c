#include "keypad.h"

static GPIO_TypeDef *row_ports[4] = { KEYPAD_ROW1_PORT, KEYPAD_ROW2_PORT, KEYPAD_ROW3_PORT, KEYPAD_ROW4_PORT };
static uint16_t       row_pins[4]  = { KEYPAD_ROW1_PIN,  KEYPAD_ROW2_PIN,  KEYPAD_ROW3_PIN,  KEYPAD_ROW4_PIN };

static GPIO_TypeDef *col_ports[4] = { KEYPAD_COL1_PORT, KEYPAD_COL2_PORT, KEYPAD_COL3_PORT, KEYPAD_COL4_PORT };
static uint16_t       col_pins[4]  = { KEYPAD_COL1_PIN,  KEYPAD_COL2_PIN,  KEYPAD_COL3_PIN,  KEYPAD_COL4_PIN };

static const char keymap[4][4] = {
    {'1', '2', '3', 'A'},
    {'4', '5', '6', 'B'},
    {'7', '8', '9', 'C'},
    {'*', '0', '#', 'D'}
};

/* edge-detection state for non-blocking debounce */
static uint8_t  key_was_down = 0;
static uint32_t last_event_tick = 0;

static void all_rows_high(void)
{
    for (uint8_t i = 0; i < 4; i++)
    {
        HAL_GPIO_WritePin(row_ports[i], row_pins[i], GPIO_PIN_SET);
    }
}

/* Non-blocking. Call once per main-loop iteration.
 * Returns the pressed key once per press (fires on the initial edge only),
 * or 0 if nothing new. No HAL_Delay, no blocking wait-for-release. */
char keypad_poll(void)
{
    all_rows_high();

    int8_t found_r = -1, found_c = -1;

    for (uint8_t r = 0; r < 4 && found_r < 0; r++)
    {
        HAL_GPIO_WritePin(row_ports[r], row_pins[r], GPIO_PIN_RESET);

        for (uint8_t c = 0; c < 4; c++)
        {
            if (HAL_GPIO_ReadPin(col_ports[c], col_pins[c]) == GPIO_PIN_RESET)
            {
                found_r = r;
                found_c = c;
                break;
            }
        }

        HAL_GPIO_WritePin(row_ports[r], row_pins[r], GPIO_PIN_SET);
    }

    if (found_r < 0)
    {
        key_was_down = 0;
        return 0;
    }

    if (key_was_down)
    {
        return 0;
    }

    if ((HAL_GetTick() - last_event_tick) < KEYPAD_DEBOUNCE_MS)
    {
        return 0;
    }

    key_was_down = 1;
    last_event_tick = HAL_GetTick();

    return keymap[found_r][found_c];
}

#ifndef LED_H
#define LED_H

#include "main.h"

// Function Prototypes
void led_signal_authorised(void);
void led_off(void);
void led_start_blink(uint32_t duration_ms, uint32_t interval_ms);

void led_tick_1ms(void);

#endif

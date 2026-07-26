#ifndef OUTPUT_H
#define OUTPUT_H

#include "main.h"
#include "fsm.h"

// Function Prototypes
void motor_open(Direction_t dir);
void motor_close(void);
void led_signal_authorised(void);
void led_off(void);
void led_start_blink(uint32_t duration_ms, uint32_t interval_ms);
void buzzer_alert(uint32_t duration_ms);
void lcd_print(const char *line1, const char *line2);

/* Continuously and non-blocking update your LED flashing status
 * and buzzer in the main loop. */
void io_actuators_process(void);

#endif

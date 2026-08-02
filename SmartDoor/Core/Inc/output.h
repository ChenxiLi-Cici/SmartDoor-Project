#ifndef OUTPUT_H
#define OUTPUT_H

#include "main.h"
#include "fsm.h"

#include "motor.h"
#include "led.h"
#include "buzzer.h"
#include "display.h"

/* Called once per ms from the interrupt.
 * update the timer variable of LED, buzzer and motor. */
void output_tick_1ms(void);


#endif

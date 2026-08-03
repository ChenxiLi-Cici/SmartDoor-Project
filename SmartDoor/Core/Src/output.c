#include "output.h"

/* Called every 1 ms by the TIM2 interrupt.
 * Passes the tick on to the LED, buzzer and motor. */
void output_tick_1ms(void) {
	led_tick_1ms();
	buzzer_tick_1ms();
	motor_tick_1ms();
}

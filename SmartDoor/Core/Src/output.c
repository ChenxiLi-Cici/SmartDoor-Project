#include "output.h"

void output_tick_1ms(void) {
	led_tick_1ms();
	buzzer_tick_1ms();
	motor_tick_1ms();
}

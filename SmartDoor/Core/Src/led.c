#include "led.h"

// the pin of the red alarm LED
#define ALARM_LED_GPIO_Port   LED_D3_GPIO_Port
#define ALARM_LED_Pin         LED_D3_Pin


//  timer variables

// How many ms will the red light continue to flash in total
static volatile uint32_t led_blink_timeout = 0;
// A fixed interval of flashing
static volatile uint32_t led_blink_interval = 0;
// How many ms are there until the next flip
static volatile uint32_t led_ms_before_next_flip = 0;

/* Called once per ms from the timer interrupt. Counts the blink down and
 * toggles the alarm LED every led_blink_interval */
void led_tick_1ms(void) {
	// alarm LED blinking
	if (led_blink_timeout > 0) {
		led_blink_timeout--;

		if (led_ms_before_next_flip > 0) {
			led_ms_before_next_flip--;
		}
		if (led_ms_before_next_flip == 0) {
			HAL_GPIO_TogglePin(ALARM_LED_GPIO_Port, ALARM_LED_Pin);
			led_ms_before_next_flip = led_blink_interval;
		}

		/* The state in which the light stops when the flashing ends is random,
		 * so it is forcibly turned off when the blink is over. */
		if (led_blink_timeout == 0) {
			HAL_GPIO_WritePin(ALARM_LED_GPIO_Port, ALARM_LED_Pin, GPIO_PIN_RESET);
		}
	}
}

// Light up the green light LD2
void led_signal_authorised(void) {
    HAL_GPIO_WritePin(LD2_GPIO_Port, LD2_Pin, GPIO_PIN_SET);
}

// Both the green and red lights go out
void led_off(void) {
	// Clear the blink_timeout first and then turn off the lights
    led_blink_timeout = 0;
    HAL_GPIO_WritePin(LD2_GPIO_Port, LD2_Pin, GPIO_PIN_RESET);
    HAL_GPIO_WritePin(ALARM_LED_GPIO_Port, ALARM_LED_Pin, GPIO_PIN_RESET);
}

// Red LED start to blink
void led_start_blink(uint32_t duration_ms, uint32_t interval_ms) {
    led_blink_timeout = duration_ms;
    led_blink_interval = interval_ms;
    led_ms_before_next_flip = interval_ms;
    HAL_GPIO_WritePin(ALARM_LED_GPIO_Port, ALARM_LED_Pin, GPIO_PIN_SET);
}

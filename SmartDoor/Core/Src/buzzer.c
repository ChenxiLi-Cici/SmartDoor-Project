#include "buzzer.h"

//  timer variables

// How many more ms does the buzzer need to sound
static volatile uint32_t buzzer_timeout = 0;

/////
#define BUZZER_PERIOD_MS 16

// Where we are inside the current buzzer pulse period
static uint8_t buzzer_phase = 0;

/////


void buzzer_tick_1ms(void) {
//	/* buzzer square wave */
//	if (buzzer_timeout > 0) {
//		buzzer_timeout--;
//		// generate the square wave
//		HAL_GPIO_TogglePin(Buzzer_GPIO_Port, Buzzer_Pin);
//
//		if (buzzer_timeout == 0) {
//			HAL_GPIO_WritePin(Buzzer_GPIO_Port, Buzzer_Pin, GPIO_PIN_RESET);
//		}
//	}

	/////
	if (buzzer_timeout > 0) {
		buzzer_timeout--;

		buzzer_phase++;
		if (buzzer_phase >= BUZZER_PERIOD_MS) {
			buzzer_phase = 0;
			HAL_GPIO_WritePin(Buzzer_GPIO_Port, Buzzer_Pin, GPIO_PIN_SET);
		} else {
			HAL_GPIO_WritePin(Buzzer_GPIO_Port, Buzzer_Pin, GPIO_PIN_RESET);
		}

		if (buzzer_timeout == 0) {
			HAL_GPIO_WritePin(Buzzer_GPIO_Port, Buzzer_Pin, GPIO_PIN_RESET);
		}
	}

	//////
}

void buzzer_off(void)
{
    buzzer_timeout = 0U;
    buzzer_phase = 0U;

    HAL_GPIO_WritePin(
        Buzzer_GPIO_Port,
        Buzzer_Pin,
        GPIO_PIN_RESET
    );
}

// Start to ring the buzzer
void buzzer_alert(uint32_t duration_ms) {
	/////
	buzzer_phase = 0;
	//////
    buzzer_timeout = duration_ms;
    HAL_GPIO_WritePin(Buzzer_GPIO_Port, Buzzer_Pin, GPIO_PIN_SET);
}

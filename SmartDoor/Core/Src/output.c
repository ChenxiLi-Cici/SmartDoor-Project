#include "output.h"
#include "lcd.h"

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
// How many more ms does the buzzer need to sound
static volatile uint32_t buzzer_timeout = 0;
// Steps left to complete the current open/close activity
static volatile uint32_t motor_steps_remaining = 0;
// How many ms until the next step
static volatile uint32_t motor_ms_before_next_step = 0;


#define MOTOR_STEP_INTERVAL_MS 3
// 2048 -> 360 degree; 2048÷4=512 -> 90 degree
#define MOTOR_STEPS_FULL_TRAVEL 512

#define MOTOR_POSITION_CLOSED        0
#define MOTOR_POSITION_ENTRY_OPEN    512
#define MOTOR_POSITION_EXIT_OPEN    -512

#define LCD_COLS 16

// At which step of the 4-step sequence (0 to 3)
static uint8_t motor_step_index = 0;
// the direction, +1 indicates moving forward, -1 indicates going back
static int8_t motor_step_dir = 1;

static volatile int32_t motor_position_steps = MOTOR_POSITION_CLOSED;

// 4-step excitation sequence: B -> A -> D -> C
static void motor_apply_step(uint8_t step) {
    if (step == 0) {
        // step 1
        HAL_GPIO_WritePin(COIL_B_GPIO_Port, COIL_B_Pin, 1);
        HAL_GPIO_WritePin(COIL_D_GPIO_Port, COIL_D_Pin, 0);
        HAL_GPIO_WritePin(COIL_A_GPIO_Port, COIL_A_Pin, 0);
        HAL_GPIO_WritePin(COIL_C_GPIO_Port, COIL_C_Pin, 0);
    } else if (step == 1) {
        // step 2
        HAL_GPIO_WritePin(COIL_B_GPIO_Port, COIL_B_Pin, 0);
        HAL_GPIO_WritePin(COIL_D_GPIO_Port, COIL_D_Pin, 0);
        HAL_GPIO_WritePin(COIL_A_GPIO_Port, COIL_A_Pin, 1);
        HAL_GPIO_WritePin(COIL_C_GPIO_Port, COIL_C_Pin, 0);
    } else if (step == 2) {
        // step 3
        HAL_GPIO_WritePin(COIL_B_GPIO_Port, COIL_B_Pin, 0);
        HAL_GPIO_WritePin(COIL_D_GPIO_Port, COIL_D_Pin, 1);
        HAL_GPIO_WritePin(COIL_A_GPIO_Port, COIL_A_Pin, 0);
        HAL_GPIO_WritePin(COIL_C_GPIO_Port, COIL_C_Pin, 0);
    } else if (step == 3) {
        // step 4
        HAL_GPIO_WritePin(COIL_B_GPIO_Port, COIL_B_Pin, 0);
        HAL_GPIO_WritePin(COIL_D_GPIO_Port, COIL_D_Pin, 0);
        HAL_GPIO_WritePin(COIL_A_GPIO_Port, COIL_A_Pin, 0);
        HAL_GPIO_WritePin(COIL_C_GPIO_Port, COIL_C_Pin, 1);
    }
}

// After opening/closing the door, cut off the power
static void motor_release(void) {
    HAL_GPIO_WritePin(COIL_A_GPIO_Port, COIL_A_Pin, 0);
    HAL_GPIO_WritePin(COIL_B_GPIO_Port, COIL_B_Pin, 0);
    HAL_GPIO_WritePin(COIL_C_GPIO_Port, COIL_C_Pin, 0);
    HAL_GPIO_WritePin(COIL_D_GPIO_Port, COIL_D_Pin, 0);
}


// Advance the stepper from the 1 ms timer interrupt
void output_tick_1ms(void) {
	/*  alarm LED blinking */
	if (led_blink_timeout > 0) {
		led_blink_timeout--;

		if (led_ms_before_next_flip > 0) {
			led_ms_before_next_flip--;
		}
		if (led_ms_before_next_flip == 0) {
			HAL_GPIO_TogglePin(ALARM_LED_GPIO_Port, ALARM_LED_Pin);
			led_ms_before_next_flip = led_blink_interval;
		}

		if (led_blink_timeout == 0) {
			HAL_GPIO_WritePin(ALARM_LED_GPIO_Port, ALARM_LED_Pin, GPIO_PIN_RESET);
		}
	}

	/* buzzer square wave */
	if (buzzer_timeout > 0) {
		buzzer_timeout--;
		// generate the square wave
		HAL_GPIO_TogglePin(Buzzer_GPIO_Port, Buzzer_Pin);

		if (buzzer_timeout == 0) {
			HAL_GPIO_WritePin(Buzzer_GPIO_Port, Buzzer_Pin, GPIO_PIN_RESET);
		}
	}

	/* stepper motor */
    if (motor_steps_remaining == 0) {
        return;
    }

    if (motor_ms_before_next_step > 0) {
        motor_ms_before_next_step--;
    }

    if (motor_ms_before_next_step == 0) {
    	// the index of the next step
        int idx = motor_step_index + motor_step_dir;
        if (idx < 0) {
            idx = 3;
        } else if (idx > 3) {
            idx = 0;
        }
        motor_step_index = idx;

        motor_apply_step(motor_step_index);
        motor_position_steps += motor_step_dir;
        motor_steps_remaining--;
        motor_ms_before_next_step = MOTOR_STEP_INTERVAL_MS;

        if (motor_steps_remaining == 0) {
            motor_release();
        }
    }
}


static void motor_move_to(int32_t target_position)
{
    int32_t difference;

    /*
     * Temporarily stop the current movement while the new
     * direction and distance are calculated.
     */
    motor_steps_remaining = 0;

    difference = target_position - motor_position_steps;

    if (difference > 0) {
        motor_step_dir = 1;
        motor_steps_remaining = (uint32_t)difference;
    }
    else if (difference < 0) {
        motor_step_dir = -1;
        motor_steps_remaining = (uint32_t)(-difference);
    }
    else {
        motor_ms_before_next_step = 0;
        motor_release();
        return;
    }

    motor_ms_before_next_step = MOTOR_STEP_INTERVAL_MS;
}

void motor_open(Direction_t dir)
{
    if (dir == DIR_ENTRY) {
        motor_move_to(MOTOR_POSITION_ENTRY_OPEN);
    }
    else {
        motor_move_to(MOTOR_POSITION_EXIT_OPEN);
    }
}

void motor_close(void)
{
    motor_move_to(MOTOR_POSITION_CLOSED);
}

// Light up the green light LD2
void led_signal_authorised(void) {
    HAL_GPIO_WritePin(LD2_GPIO_Port, LD2_Pin, GPIO_PIN_SET);
}

// Both the green and red lights go out
void led_off(void) {
	// Clear first and then turn off the lights
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

// Start to ring the buzzer
void buzzer_alert(uint32_t duration_ms) {
    buzzer_timeout = duration_ms;
    HAL_GPIO_WritePin(Buzzer_GPIO_Port, Buzzer_Pin, GPIO_PIN_SET);
}

//
void lcd_print(const char *line1, const char *line2) {
	const char *lines[2] = { line1, line2 };

	for (int i = 0; i < 2; i++) {
		const char *text = lines[i];
		uint8_t written = 0;

		// Move the cursor to the beginning of the line and then output
		LCD_SetCursor(i, 0);

		while (text != 0 && *text != '\0' && written < LCD_COLS) {
			LCD_SendData((uint8_t)(*text));
			text++;
			written++;
		}
		// Fill in Spaces at the end
		while (written < LCD_COLS) {
			LCD_SendData(' ');
			written++;
		}
	}

}


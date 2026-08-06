#include "motor.h"
#include "fsm.h"
#include <stdbool.h>
//  timer variables

// Steps left to complete the current open/close activity
static volatile uint32_t motor_steps_remaining = 0;
// How many ms until the next step
static volatile uint32_t motor_ms_before_next_step = 0;
static volatile bool motor_release_pending = false;
#define MOTOR_STEP_INTERVAL_MS 3

/* Manually adjust the initial position of motor */

// Manual SW1 adjustment speed
#define MOTOR_MANUAL_STEP_INTERVAL_MS 8U
// How many ms a button must be held before it counts as pressed
#define SW1_DEBOUNCE_MS 10U
// True while the admin is adjust motor with SW1 / SW2
static volatile bool motor_manual_adjust_active = false;
// Debounce counters
static volatile uint8_t sw1_debounce_counter = 0U;
static volatile uint8_t sw2_debounce_counter = 0U;
static void motor_manual_adjust_tick_1ms(void);

// 2048 -> 360 degree; 2048÷4=512 -> 90 degree
#define MOTOR_STEPS_FULL_TRAVEL 512

// Absolute positions, in steps from closed.
#define MOTOR_POSITION_CLOSED        0
#define MOTOR_POSITION_ENTRY_OPEN    512
#define MOTOR_POSITION_EXIT_OPEN    -512

// At which step of the 4-step sequence (0 to 3)
static uint8_t motor_step_index = 0;
// the direction, +1 indicates moving forward, -1 indicates going back
static volatile int8_t motor_step_dir = 1;

// The current position, in steps from closed
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
void motor_tick_1ms(void) {
	/* stepper motor */
    motor_manual_adjust_tick_1ms();

    if (!motor_manual_adjust_active &&
        motor_steps_remaining == 0) {

        /* Do not release the coils in the same interrupt as the final step.
         * Keep that final phase active for one complete step interval so the
         * rotor has time to reach it, then remove power. */
        if (motor_release_pending) {
            if (motor_ms_before_next_step > 0U) {
                motor_ms_before_next_step--;
            }

            if (motor_ms_before_next_step == 0U) {
                motor_release();
                motor_release_pending = false;
            }
        }

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

        // Manual mode does not track position because we we want to adjust the position where it closes
        if (motor_manual_adjust_active) {
            motor_ms_before_next_step = MOTOR_MANUAL_STEP_INTERVAL_MS;
        }
        else {
            motor_position_steps += motor_step_dir;
            motor_steps_remaining--;
            motor_ms_before_next_step = MOTOR_STEP_INTERVAL_MS;

            if (motor_steps_remaining == 0) {
                motor_release_pending = true;
            }
        }
    }
}   



// Move the motor to an absolute position, given in steps from the closed position
static void motor_move_to(int32_t target_position)
{
    int32_t difference;
    // While the admin is adjusting the motor by hand, ignore automatic moves
    if (fsm_get_state() == IDLE &&
        (motor_manual_adjust_active || 
            HAL_GPIO_ReadPin(SW1_GPIO_Port, SW1_Pin) == GPIO_PIN_SET||
            HAL_GPIO_ReadPin(SW2_GPIO_Port, SW2_Pin) == GPIO_PIN_SET)) {
        return;
    }

    // The manually selected position becomes the new closed position.
    if (motor_manual_adjust_active) {
        motor_manual_adjust_active = false;
        sw1_debounce_counter = 0U;
        sw2_debounce_counter = 0U;
        motor_position_steps = MOTOR_POSITION_CLOSED;
        motor_release_pending = false;
        motor_release();
    }

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
        if (!motor_release_pending) {
            motor_ms_before_next_step = 0;
            motor_release();
        }
        return;
    }
    motor_release_pending = false;
    motor_ms_before_next_step = MOTOR_STEP_INTERVAL_MS;
}


// Turn the motor according to the input direction to open the door
void motor_open(Direction_t dir)
{
    if (dir == DIR_ENTRY) {
        motor_move_to(MOTOR_POSITION_ENTRY_OPEN);
    }
    else {
        motor_move_to(MOTOR_POSITION_EXIT_OPEN);
    }
}

// Return the door to the closed position
void motor_close(void)
{
    motor_move_to(MOTOR_POSITION_CLOSED);
}


// press SW1 or SW2 to adjust the motor manually while the door is idle.
static void motor_manual_adjust_tick_1ms(void)
{
	// Manual adjustment is only allowed in idle.
    if (fsm_get_state() != IDLE) {
        sw1_debounce_counter = 0U;
        sw2_debounce_counter = 0U;
        if (motor_manual_adjust_active) {
            motor_manual_adjust_active = false;
            motor_steps_remaining = 0U;
            motor_ms_before_next_step = 0U;
            motor_release_pending = false;

            motor_position_steps = MOTOR_POSITION_CLOSED;

            motor_release();
        }

        return;
    }

    /* Debounce by counting up while the button is held and down while it is released
	 *  pressed once the counter reaches SW1_DEBOUNCE_MS, released
	 * once it is back to zero. */
    if (HAL_GPIO_ReadPin(
            SW1_GPIO_Port,
            SW1_Pin
        ) == GPIO_PIN_SET) {

        if (sw1_debounce_counter <
            SW1_DEBOUNCE_MS) {

            sw1_debounce_counter++;
        }
    }
    else if (sw1_debounce_counter > 0U) {
        sw1_debounce_counter--;
    }

    if (HAL_GPIO_ReadPin(
            SW2_GPIO_Port,
            SW2_Pin
        ) == GPIO_PIN_SET) {

        if (sw2_debounce_counter <
            SW1_DEBOUNCE_MS) {

            sw2_debounce_counter++;
        }
    }
    else if (sw2_debounce_counter > 0U) {
        sw2_debounce_counter--;
    }   

    // SW2 turns the motor backwards.
    if (sw2_debounce_counter == SW1_DEBOUNCE_MS) {

        if (!motor_manual_adjust_active ||
            motor_step_dir != -1) {

            motor_steps_remaining = 0U;
            motor_step_dir = -1;

            motor_ms_before_next_step = 0U;
            motor_release_pending = false;

            motor_manual_adjust_active = true;
        }
        return;
    }

    // Stop after SW2 has been released for 10 ms.
    if (motor_manual_adjust_active &&
        motor_step_dir == -1) {

        if (sw2_debounce_counter == 0U) {
            motor_manual_adjust_active = false;
            motor_steps_remaining = 0U;
            motor_ms_before_next_step = 0U;
            motor_release_pending = false;

            motor_position_steps = MOTOR_POSITION_CLOSED;
            motor_release();
        }

        return;
    }
    
    // SW1 turns the motor forwards.
    if (!motor_manual_adjust_active &&
        sw1_debounce_counter == SW1_DEBOUNCE_MS) {

        motor_steps_remaining = 0U;
        motor_step_dir = 1;

        // Perform the first step immediately
        motor_ms_before_next_step = 0U;
        motor_release_pending = false;
        motor_manual_adjust_active = true;
    }


    // Stop after SW1 has been released for 10 ms.
    else if (motor_manual_adjust_active && sw1_debounce_counter == 0U) {

        motor_manual_adjust_active = false;
        motor_steps_remaining = 0U;
        motor_ms_before_next_step = 0U;
        motor_release_pending = false;
        motor_position_steps = MOTOR_POSITION_CLOSED;

        motor_release();
    }
}

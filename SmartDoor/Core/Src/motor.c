#include "motor.h"

//  timer variables

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
void motor_tick_1ms(void) {
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

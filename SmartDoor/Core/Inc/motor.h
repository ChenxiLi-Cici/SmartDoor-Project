#ifndef MOTOR_H
#define MOTOR_H

#include "main.h"
#include "fsm.h"

// Function Prototypes

// Turn the motor according to the input direction to open the door
void motor_open(Direction_t dir);

// Return the door to the closed position
void motor_close(void);

// Advance the stepper from the 1 ms timer interrupt
void motor_tick_1ms(void);

#endif

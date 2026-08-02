#ifndef MOTOR_H
#define MOTOR_H

#include "main.h"
#include "fsm.h"

// Function Prototypes
void motor_open(Direction_t dir);
void motor_close(void);

void motor_tick_1ms(void);

#endif

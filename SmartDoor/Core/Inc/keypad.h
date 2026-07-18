#ifndef KEYPAD_H
#define KEYPAD_H

#include "main.h"

#define KEYPAD_ROW1_PORT  GPIOB
#define KEYPAD_ROW1_PIN   GPIO_PIN_11
#define KEYPAD_ROW2_PORT  GPIOB
#define KEYPAD_ROW2_PIN   GPIO_PIN_12
#define KEYPAD_ROW3_PORT  GPIOB
#define KEYPAD_ROW3_PIN   GPIO_PIN_13
#define KEYPAD_ROW4_PORT  GPIOB
#define KEYPAD_ROW4_PIN   GPIO_PIN_14

#define KEYPAD_COL1_PORT  GPIOA
#define KEYPAD_COL1_PIN   GPIO_PIN_8
#define KEYPAD_COL2_PORT  GPIOA
#define KEYPAD_COL2_PIN   GPIO_PIN_9
#define KEYPAD_COL3_PORT  GPIOA
#define KEYPAD_COL3_PIN   GPIO_PIN_10
#define KEYPAD_COL4_PORT  GPIOA
#define KEYPAD_COL4_PIN   GPIO_PIN_11

#define KEYPAD_DEBOUNCE_MS  200

/* Non-blocking, edge-triggered poll. Call once per main-loop iteration.
 * Returns the pressed key ('0'-'9','A'-'D','*','#') once per press,
 * or 0 if nothing new. Matches the Part 2 io_sensors.c interface
 * (keypad_poll) from the FSM design doc. */
char keypad_poll(void);

#endif /* KEYPAD_H */

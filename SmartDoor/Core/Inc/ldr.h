#ifndef LDR_H
#define LDR_H

#include <stdbool.h>
#include <stdint.h>
#include "fsm.h"

typedef bool ldrState_t;

#define LDR_CLEAR false
#define LDR_BLOCK true

/* Reset all LDR software state. */
void ldr_init(void);

/* Enable or disable passage detection. */
void ldr_arm(bool armed);

/* Read the two raw ADC values. */
bool ldr_read_raw(uint16_t *ldr1_value, uint16_t *ldr2_value);

/* True only when neither LDR is blocked. */
bool ldr_path_is_clear(void);

/* Process LDR readings and return a door event. */
Event_t ldr_poll(void);

#endif /* LDR_H */

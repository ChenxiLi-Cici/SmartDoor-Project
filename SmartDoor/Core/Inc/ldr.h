#ifndef LDR_H
#define LDR_H

#include <stdbool.h>
#include <stdint.h>
#include "fsm.h"

typedef bool ldrState_t;

// Use a Boolean type because each LDR has only two logical states.
// false represents a clear light path and true represents a blocked path.
#define LDR_CLEAR false
#define LDR_BLOCK true

// Reset the edge detection, debounce and passage sequence variables.
// This function should be called once during program initialisation.
void ldr_init(void);

// Enable or disable LDR passage detection.
// Calling this function also starts a new passage sequence.
void ldr_arm(bool armed);

// Read the raw ADC value from both LDRs.
// The two pointer arguments are used to return the ADC results to the caller.
// Return false if either ADC conversion fails.
bool ldr_read_raw(uint16_t *ldr1_value, uint16_t *ldr2_value);

// Return true only when the latest stable state of both LDRs is CLEAR.
// The FSM uses this function before it allows the door to close.
bool ldr_path_is_clear(void);

// Read, debounce and process both LDRs once.
// Return the event produced by the passage sequence, or EVT_NONE if no event occurred.
Event_t ldr_poll(void);

#endif // LDR_H

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

// Read the raw ADC value from both LDRs.
// The two pointer arguments are used to return the ADC results to the caller.
// Return false if either ADC conversion fails.
bool ldr_read_raw(uint16_t *ldr1_value, uint16_t *ldr2_value);

// Return true only when the latest stable state of both LDRs is CLEAR.
// The FSM uses this function before it allows the door to close.
bool ldr_path_is_clear(void);

// Return true only when both LDRs have remained continuously CLEAR for the
// requested duration. Any new obstruction restarts the clear interval.
bool ldr_path_has_been_clear_for(uint32_t duration_ms);

// Return true when the latest stable state of the outside entry sensor is BLOCK.
// The FSM uses this after card authorisation so a person already at LDR1 does
// not need to step away and trigger a second edge before the door opens.
bool ldr_entry_sensor_is_blocked(void);

// Lock passage recognition to the direction currently owned by the FSM.
// The opposite sensor is still monitored for closing safety, but it cannot
// start a new passage or reverse the motor until the lock is released.
void ldr_lock_direction(Direction_t direction);

// Return to automatic direction detection after the owned passage and
// closing cycle have both finished.
void ldr_unlock_direction(void);

// Read, debounce and process both LDRs once.
// Return the event produced by the passage sequence, or EVT_NONE if no event occurred.
Event_t ldr_poll(void);

#endif // LDR_H

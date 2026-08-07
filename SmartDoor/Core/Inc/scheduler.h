#ifndef SCHEDULER_H
#define SCHEDULER_H

#include "main.h"
#include <stdbool.h>

/* Set a unlock event: the door stays unlocked for
 * `duration_min` minutes starting at `hour:minute` on `month/day`.
 * Assumption: Events are limited to a single day, so start + duration must not run past
 * midnight. The year is not considered. */
void scheduler_set_event(uint8_t month, uint8_t day,
                         uint8_t hour, uint8_t minute,
                         uint16_t duration_min);

// Cancel the configured event.
void scheduler_disable(void);

// Read back the configured event so that the admin menu can show what is already scheduled.
bool scheduler_get_event(uint8_t *month, uint8_t *day,
                         uint8_t *hour, uint8_t *minute,
                         uint16_t *duration_min);

// Set the RTC's current time, in 24-hour HH:MM.
void scheduler_set_clock(uint8_t hour, uint8_t minute);

// Set the RTC's current date.
void scheduler_set_date(uint8_t month, uint8_t day);

// Read the current clock time from the RTC.
void scheduler_get_time(uint8_t *hour, uint8_t *minute);

// Read the current date from the RTC.
void scheduler_get_date(uint8_t *month, uint8_t *day);

// force close the opening scheduler
void scheduler_force_close(void);

// Scheduler Event Interface
bool scheduler_check_start(void);
bool scheduler_check_end(void);

#endif /* SCHEDULER_H */

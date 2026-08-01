#ifndef SCHEDULER_H
#define SCHEDULER_H

#include "main.h"
#include <stdbool.h>

/* Configure a unlock window using real time
 * e.g. scheduler_set_window(9, 0, 9, 30) unlocks the door every day
 * from 09:00 to 09:30
 */
void scheduler_set_window(uint8_t start_hour, uint8_t start_min,
                           uint8_t end_hour, uint8_t end_min);

// Set the RTC's current time, in 24-hour HH:MM.
void scheduler_set_clock(uint8_t hour, uint8_t minute);

// Cancel the configured window.
void scheduler_disable(void);

// Scheduler Event Interface
bool scheduler_check_start(void);
bool scheduler_check_end(void);

#endif /* SCHEDULER_H */

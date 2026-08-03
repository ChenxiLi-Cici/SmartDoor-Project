#ifndef BUZZER_H
#define BUZZER_H

#include "main.h"

// Function Prototypes
// Start to ring the buzzer
void buzzer_alert(uint32_t duration_ms);
// Silence the buzzer
void buzzer_off(void);
/* Called once per ms from the timer interrupt. Counts the buzzer_timeout down and
 * silences the buzzer when the time is up. */
void buzzer_tick_1ms(void);

#endif

#include "scheduler.h"
#include <stdio.h>

// Declared by CubeMX in main.c
extern RTC_HandleTypeDef hrtc;

/* Window boundaries
 * In order to make the comparison easier,
 * the time is stored as the number of minutes starting from 0:00. */
static uint16_t window_start_min = 0;
static uint16_t window_end_min   = 0;

// Has the administrator set up a unlock window
static bool window_configured = false;

static bool currently_unlocked = false;

// Calculate which minute of the day the "hour:minute" represents
static uint16_t minutes_of_day(uint8_t hour, uint8_t minute) {
    return (uint16_t)hour * 60 + minute;
}

// Reads the RTC and returns it as minutes since midnight.
static uint16_t current_minute_of_day(void) {
    uint8_t hour   = 0;
    uint8_t minute = 0;

    scheduler_get_time(&hour, &minute);

    return minutes_of_day(hour, minute);
}

// Is the current time inside [window_start_min, window_end_min)
static bool is_in_window(uint16_t now) {
    if (window_start_min <= window_end_min) {
        return now >= window_start_min && now < window_end_min;
    } else {
    	// If the window crosses midnight (start > end)
        return now >= window_start_min || now < window_end_min;
    }
}

// Configure a new unlock window
void scheduler_set_window(uint8_t start_hour, uint8_t start_min,
                           uint8_t end_hour, uint8_t end_min) {
    window_start_min   = minutes_of_day(start_hour, start_min);
    window_end_min     = minutes_of_day(end_hour, end_min);
    window_configured  = true;
    // Ensure that after the window is reset, it always be judged from the ununlocked state
    currently_unlocked = false;

    printf("Scheduler: window set %02u:%02u - %02u:%02u\r\n",
           start_hour, start_min, end_hour, end_min);
}


// Set the RTC's current time, in 24-hour HH:MM.
void scheduler_set_clock(uint8_t hour, uint8_t minute) {
    RTC_TimeTypeDef sTime = {0};

    sTime.Hours          = hour;
    sTime.Minutes        = minute;
    sTime.Seconds        = 0;

    if (HAL_RTC_SetTime(&hrtc, &sTime, RTC_FORMAT_BIN) != HAL_OK) {
		printf("Scheduler: failed to set clock\r\n");
		return;
	}

    /* Since we have modified the absolute time,
     * whether it is unlock or not needs to be rejudged
    */
    currently_unlocked = false;

    printf("Scheduler: clock set to %02u:%02u\r\n", hour, minute);
}



// called when the administrator wants to cancel the scheduled unlock
void scheduler_disable(void) {
    window_configured  = false;
    currently_unlocked = false;
}


/* Read the configured window so that the admin_menu can pre-fill the LCD.
 * Returns false if no window has been configured yet. */
bool scheduler_get_window(uint8_t *start_hour, uint8_t *start_min,
                          uint8_t *end_hour, uint8_t *end_min) {
    *start_hour = (uint8_t)(window_start_min / 60);
    *start_min  = (uint8_t)(window_start_min % 60);
    *end_hour   = (uint8_t)(window_end_min / 60);
    *end_min    = (uint8_t)(window_end_min % 60);

    return window_configured;
}

// Read the current clock time from the RTC.
void scheduler_get_time(uint8_t *hour, uint8_t *minute) {
    // Declare an empty structure to store time (hours, minutes, seconds)
	RTC_TimeTypeDef sTime = {0};

	/* Declare an empty structure to store date.
	 Even though we don't care about the date, HAL requires calling
	 GetDate to unlock the shadow register after GetTime -- otherwise
	 the shadow register (which latch both time and date) won't
	 update. So we must call GetDate, and therefore need a structure
	 ready to receive the date.
	*/
	RTC_DateTypeDef sDate = {0};

	// Get the time
	/* RTC_FORMAT_BIN: Convert time data into a regular binary integer,
	   so that it can be directly used for + - * /
	*/
	HAL_RTC_GetTime(&hrtc, &sTime, RTC_FORMAT_BIN);

	HAL_RTC_GetDate(&hrtc, &sDate, RTC_FORMAT_BIN);


    *hour   = sTime.Hours;
    *minute = sTime.Minutes;
}


// return 1 exactly when the current time crosses into the configured window.
bool scheduler_check_start(void) {
	/* No configured window or
	 *  it is already in the unlocked state (true has been triggered once before)
	 */
    if (!window_configured || currently_unlocked) {
        return false;
    }
    // Read the current time from RTC and see if it falls within the configuration window.
    if (is_in_window(current_minute_of_day())) {
        currently_unlocked = true;
        printf("Scheduler: window START\r\n");
        return true;
    }
    return false;
}

// return 1 exactly when the current time crosses out of the configured window.
bool scheduler_check_end(void) {
	// No configured window or
	// It is not unlocked yet
    if (!window_configured || !currently_unlocked) {
        return false;
    }

    // It's no longer within the window range now.
    if (!is_in_window(current_minute_of_day())) {
        currently_unlocked = false;
        printf("Scheduler: window END\r\n");
        return true;
    }
    return false;
}

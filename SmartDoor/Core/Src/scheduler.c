#include "scheduler.h"
#include <stdio.h>

// Declared by CubeMX in main.c
extern RTC_HandleTypeDef hrtc;

// The start date of the event
static uint8_t  event_month = 0;
static uint8_t  event_day = 0;

// Start time, in minutes since midnight
static uint16_t event_start_min = 0;
// The duration of the scheduled event, in minutes
static uint16_t event_duration_min = 0;

// Has the event been configured
static bool event_configured = false;

// True while the scheduler is currently holding the door open
static bool schedule_holds_door_open = false;

// Calculate minutes since midnight the "hour:minute" represents
static uint16_t minutes_since_midnight(uint8_t hour, uint8_t minute) {
    return (uint16_t)hour * 60 + minute;
}

// get the current clock time from RTC
static void get_current_time(uint8_t *month, uint8_t *day, uint16_t *minute_of_day)
{
    RTC_TimeTypeDef sTime = {0};
    RTC_DateTypeDef sDate = {0};

    HAL_RTC_GetTime(&hrtc, &sTime, RTC_FORMAT_BIN);
    HAL_RTC_GetDate(&hrtc, &sDate, RTC_FORMAT_BIN);

    *month = sDate.Month;
    *day = sDate.Date;
    *minute_of_day = minutes_since_midnight(sTime.Hours, sTime.Minutes);
}

/* Is the event running right now
 * We assumed that events never cross midnight */
static bool event_is_running(void)
{
    uint8_t month = 0;
    uint8_t day = 0;
    uint16_t now_min = 0;

    get_current_time(&month, &day, &now_min);

    // The date must match
    if (month != event_month || day != event_day) {
        return false;
    }

    // the time must fall inside [start, start + duration).
    return (now_min >= event_start_min) &&
           (now_min <  event_start_min + event_duration_min);
}

// Configure a new unlock event
void scheduler_set_event(uint8_t month, uint8_t day,
                         uint8_t hour, uint8_t minute,
                         uint16_t duration_min)
{
    event_month = month;
    event_day = day;
    event_start_min = minutes_since_midnight(hour, minute);
    event_duration_min = duration_min;
    event_configured = true;

    printf("Scheduler: event set %02u/%02u %02u:%02u for %u min\r\n",
           day, month, hour, minute, duration_min);
}

// Cancel the configured event
void scheduler_disable(void)
{
    event_configured = false;
    printf("Scheduler: event cancelled\r\n");
}

// force close the opening scheduler
void scheduler_force_close(void)
{
    if (schedule_holds_door_open) {
        event_configured = false;
        schedule_holds_door_open = false;
        printf("Scheduler: event cancelled\r\n");
    }
}

// Read back the event so the admin menu can show what is already scheduled
bool scheduler_get_event(uint8_t *month, uint8_t *day,
                         uint8_t *hour, uint8_t *minute,
                         uint16_t *duration_min)
{
    *month = event_month;
    *day = event_day;
    *hour = (uint8_t)(event_start_min / 60);
    *minute = (uint8_t)(event_start_min % 60);
    *duration_min = event_duration_min;

    return event_configured;
}

// Set the RTC's current time (HH:MM).
void scheduler_set_clock(uint8_t hour, uint8_t minute)
{
    RTC_TimeTypeDef sTime = {0};

    sTime.Hours = hour;
    sTime.Minutes = minute;
    sTime.Seconds = 0;

    if (HAL_RTC_SetTime(&hrtc, &sTime, RTC_FORMAT_BIN) != HAL_OK) {
        printf("Scheduler: failed to set clock\r\n");
        return;
    }

    printf("Scheduler: clock set to %02u:%02u\r\n", hour, minute);
}

// Set the RTC's current date.
void scheduler_set_date(uint8_t month, uint8_t day)
{
    RTC_DateTypeDef sDate = {0};

    sDate.Month = month;
    sDate.Date = day;
    // Year is not used since we assume the event is held in a single-day
    sDate.Year = 0;

    sDate.WeekDay = RTC_WEEKDAY_MONDAY;

    if (HAL_RTC_SetDate(&hrtc, &sDate, RTC_FORMAT_BIN) != HAL_OK) {
        printf("Scheduler: failed to set date\r\n");
        return;
    }

    printf("Scheduler: date set to %02u/%02u\r\n", day, month);
}

// Read the current clock time from the RTC.
void scheduler_get_time(uint8_t *hour, uint8_t *minute)
{
    uint8_t month = 0;
    uint8_t day = 0;
    uint16_t now_min = 0;

    get_current_time(&month, &day, &now_min);

    *hour = (uint8_t)(now_min / 60);
    *minute = (uint8_t)(now_min % 60);
}

// Read the current date from the RTC.
void scheduler_get_date(uint8_t *month, uint8_t *day)
{
    uint16_t now_min = 0;

    get_current_time(month, day, &now_min);
}

// return 1 exactly when the event starts, only polled from IDLE
bool scheduler_check_start(void)
{
    // Nothing configured
    if (!event_configured) {
        schedule_holds_door_open = false;
        return false;
    }

    // The event is not running
    if (!event_is_running()) {
        schedule_holds_door_open = false;
        return false;
    }

    /* If it is already opened by the scheduler,
       we don't need to raise the start signal again
    */
    if (schedule_holds_door_open) {
        return false;
    }

    schedule_holds_door_open = true;
    printf("Scheduler: event START\r\n");
    return true;
}

// return 1 exactly when the event finishes, only polled from UNLOCKED
bool scheduler_check_end(void)
{
    // The door is not currently held by the scheduler
    if (!schedule_holds_door_open) {
        return false;
    }

    // The event was cancelled, or its time is up
    if (!event_configured || !event_is_running()) {
    	// clear the two state variable
        schedule_holds_door_open = false;
        event_configured = false;
        printf("Scheduler: event END\r\n");
        return true;
    }
    return false;
}

#include "admin_menu.h"
#include "output.h"
#include "scheduler.h"
#include "fsm.h"
#include <stdbool.h>
#include <stdio.h>

// The state machine inside the menu
typedef enum {
    ADMIN_MENU_PAGE1,   /* 1.Schdl  2.Lock */
    ADMIN_MENU_PAGE2,   /* 3.Clock  4.Exit */
    ADMIN_SCHED_START,  /* typing the window start time */
    ADMIN_SCHED_END,    /* typing the window end time */
    ADMIN_SET_CLOCK,    /* setting the current time */
    ADMIN_LOCK_CONFIG   /* force the door open / restore normal */
} AdminScreen_t;

/* The welcome screen stays for 2 seconds when entering the menu */
#define ADMIN_SPLASH_MS 2000
/* How long a confirmation message stays on screen */
#define ADMIN_CONFIRM_MS 2000
/* Time entry is 4 digits: HHMM */
#define ADMIN_DIGITS 4
/* Default value used when no window has been configured yet */
#define ADMIN_DEFAULT_START_HOUR 9
#define ADMIN_DEFAULT_START_MIN  0
#define ADMIN_DEFAULT_END_HOUR   16
#define ADMIN_DEFAULT_END_MIN    30

static AdminScreen_t screen = ADMIN_MENU_PAGE1;

/* The 4 digits shown in the [HH:MM] field, and which one the next
 * keypress overwrites. */
static char    entry[ADMIN_DIGITS];
static uint8_t cursor = 0;

/* Start time held aside while the end time is being typed */
static uint8_t pending_start_hour = 0;
static uint8_t pending_start_min  = 0;

/* ---------------- time entry helpers ---------------- */

// Pre-fill the entry field with the default time
static void entry_load(uint8_t hour, uint8_t minute) {
    entry[0] = (char)('0' + hour / 10);
    entry[1] = (char)('0' + hour % 10);
    entry[2] = (char)('0' + minute / 10);
    entry[3] = (char)('0' + minute % 10);
    // cursor back at the front. Let the next input start overwriting from the first digit.
    cursor   = 0;
}

// Type the entry
static void entry_type(char digit) {
	// Write a number at the cursor position
    entry[cursor] = digit;
    // a 5th digit wraps back to the front.
    cursor = (uint8_t)((cursor + 1) % ADMIN_DIGITS);
}

// Assemble the four digits into a display format [HH:MM]
static void entry_to_text(char *out) {
    out[0] = '[';
    out[1] = entry[0];
    out[2] = entry[1];
    out[3] = ':';
    out[4] = entry[2];
    out[5] = entry[3];
    out[6] = ']';
    out[7] = '\0';
}

// .
// Turn the 4 digits into hours/minutes; false if not a valid 24-hour reading.
static bool entry_to_time(uint8_t *hour, uint8_t *minute) {
    uint8_t h = (uint8_t)((entry[0] - '0') * 10 + (entry[1] - '0'));
    uint8_t m = (uint8_t)((entry[2] - '0') * 10 + (entry[3] - '0'));

    if (h > 23 || m > 59) {
        return false;
    }

    *hour   = h;
    *minute = m;
    return true;
}


// Redraw whichever screen is currently active.
static void show_screen(void) {
    char field[8];
    char line2[20];

    switch (screen) {

    case ADMIN_MENU_PAGE1:
        lcd_print("Press 1-4 B:Next", "1.Schdl  2.Lock");
        break;

    case ADMIN_MENU_PAGE2:
        lcd_print("Press 1-4 A:Back", "3.Clock  4.Exit");
        break;

    case ADMIN_SCHED_START:
        entry_to_text(field);
        snprintf(line2, sizeof(line2), "%s   B:Nxt", field);
        lcd_print("Set Start C:Off", line2);
        break;

    case ADMIN_SCHED_END:
        entry_to_text(field);
        snprintf(line2, sizeof(line2), "%s  #:Save", field);
        lcd_print("Set End  A:Back", line2);
        break;

    case ADMIN_SET_CLOCK:
        entry_to_text(field);
        snprintf(line2, sizeof(line2), "%s  #:Save", field);
        lcd_print("Set Clock A:Back", line2);
        break;

    case ADMIN_LOCK_CONFIG:
        if (fsm_get_admin_origin() == UNLOCKED) {
            lcd_print("#:Select A:Back", ">Restore NORMAL");
        } else {
            lcd_print("#:Select A:Back", ">Set OPEN");
        }
        break;
    }
}

// public interface

void admin_menu_enter(void) {
    printf("Admin: menu opened\r\n");

    lcd_print("   ADMIN MODE", "    Welcome");
    HAL_Delay(ADMIN_SPLASH_MS);

    screen = ADMIN_MENU_PAGE1;
    show_screen();
}

void admin_menu_exit(void) {
    screen = ADMIN_MENU_PAGE1;
    printf("Admin: menu closed\r\n");
}

void admin_menu_handle_key(char key) {

    switch (screen) {

    /* main menu, page 1*/
    case ADMIN_MENU_PAGE1:
        if (key == '1') {
            uint8_t sh, sm, eh, em;
            if (!scheduler_get_window(&sh, &sm, &eh, &em)) {
                sh = ADMIN_DEFAULT_START_HOUR;
                sm = ADMIN_DEFAULT_START_MIN;
            }
            entry_load(sh, sm);
            screen = ADMIN_SCHED_START;
        } else if (key == '2') {
            screen = ADMIN_LOCK_CONFIG;
        } else if (key == 'B') {
            screen = ADMIN_MENU_PAGE2;
        }
        show_screen();
        break;

    /*  main menu, page 2 */
    case ADMIN_MENU_PAGE2:
        if (key == '3') {
            uint8_t hour, minute;
            scheduler_get_time(&hour, &minute);
            entry_load(hour, minute);
            screen = ADMIN_SET_CLOCK;
        } else if (key == '4') {
            lcd_print("    SUCCESS !", "");
            HAL_Delay(ADMIN_CONFIRM_MS);
            fsm_dispatch(EVT_ADMIN_EXIT);
            return;
        } else if (key == 'A') {
            screen = ADMIN_MENU_PAGE1;
        }
        show_screen();
        break;

    /* schedule: start time */
    case ADMIN_SCHED_START:
        if (key >= '0' && key <= '9') {
            entry_type(key);
        } else if (key == 'C') {
            scheduler_disable();
            lcd_print("Schedule", "turned OFF");
            HAL_Delay(ADMIN_CONFIRM_MS);
            screen = ADMIN_MENU_PAGE1;
        } else if (key == 'A') {
            screen = ADMIN_MENU_PAGE1;
        } else if (key == 'B') {
            uint8_t hour, minute;
            if (!entry_to_time(&hour, &minute)) {
                lcd_print("Invalid time", "Re-enter HH:MM");
                return;
            }
            pending_start_hour = hour;
            pending_start_min  = minute;

            uint8_t sh, sm, eh, em;
            if (!scheduler_get_window(&sh, &sm, &eh, &em)) {
                eh = ADMIN_DEFAULT_END_HOUR;
                em = ADMIN_DEFAULT_END_MIN;
            }
            entry_load(eh, em);
            screen = ADMIN_SCHED_END;
        }
        show_screen();
        break;

    /* schedule: end time */
    case ADMIN_SCHED_END:
        if (key >= '0' && key <= '9') {
            entry_type(key);
        } else if (key == 'A') {
            entry_load(pending_start_hour, pending_start_min);
            screen = ADMIN_SCHED_START;
        } else if (key == '#') {
            uint8_t hour, minute;
            if (!entry_to_time(&hour, &minute)) {
                lcd_print("Invalid time", "Re-enter HH:MM");
                return;
            }
            scheduler_set_window(pending_start_hour, pending_start_min,
                                 hour, minute);
            lcd_print("Schedule saved", "");
            HAL_Delay(ADMIN_CONFIRM_MS);
            screen = ADMIN_MENU_PAGE1;
        }
        show_screen();
        break;

    /* set the clock */
    case ADMIN_SET_CLOCK:
        if (key >= '0' && key <= '9') {
            entry_type(key);
        } else if (key == 'A') {
            screen = ADMIN_MENU_PAGE2;
        } else if (key == '#') {
            uint8_t hour, minute;
            if (!entry_to_time(&hour, &minute)) {
                lcd_print("Invalid time", "Re-enter HH:MM");
                return;
            }
            scheduler_set_clock(hour, minute);
            lcd_print("Clock updated", "");
            HAL_Delay(ADMIN_CONFIRM_MS);
            screen = ADMIN_MENU_PAGE2;
        }
        show_screen();
        break;

    /* lock configuration  */
    case ADMIN_LOCK_CONFIG:
        if (key == '#') {
            if (fsm_get_admin_origin() == UNLOCKED) {
                lcd_print("Restoring", "NORMAL mode");
                HAL_Delay(ADMIN_CONFIRM_MS);
                fsm_dispatch(EVT_ADMIN_SET_NORMAL);
            } else {
                lcd_print("Door set to", "OPEN mode");
                HAL_Delay(ADMIN_CONFIRM_MS);
                fsm_dispatch(EVT_ADMIN_SET_OPEN);
            }
            return;
        } else if (key == 'A') {
            screen = ADMIN_MENU_PAGE1;
        }
        show_screen();
        break;
    }
}

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
    ADMIN_SCHED_MENU,   /* 1:View  2:Set */
    ADMIN_SCHED_VIEW,   /* show the configured event */
    ADMIN_SCHED_DATE,   /* typing the event date */
    ADMIN_SCHED_START,  /* typing the event start time */
    ADMIN_SCHED_DUR,    /* typing the event duration */
    ADMIN_SET_DATE,     /* typing the current date */
    ADMIN_SET_CLOCK,    /* typing the current time */
    ADMIN_LOCK_CONFIG   /* force the door open / restore normal */
} AdminScreen_t;

// The welcome screen stays for 2 seconds when entering the menu
#define ADMIN_SPLASH_MS 2000
// How long a confirmation message stays on screen
#define ADMIN_CONFIRM_MS 2000
// Time entry is 4 digits
#define ADMIN_DIGITS 4
// Minutes in a day
#define MINUTES_PER_DAY 1440U

// Defaults time shown on the LCD
#define ADMIN_DEFAULT_START_HOUR 9
#define ADMIN_DEFAULT_START_MIN  0
#define ADMIN_DEFAULT_DUR_HOUR   1
#define ADMIN_DEFAULT_DUR_MIN    0

static AdminScreen_t screen = ADMIN_MENU_PAGE1;

// The 4 digits shown in the [xx:xx]
static char entry[ADMIN_DIGITS];
// Which entry (0 to 3) will the next key be written into
static uint8_t curr_entry_i = 0;


/* Since the Settings page has three pages in total,
 * when switching pages, the entry will be overwritten by load_entry,
 * so the time data that has been entered needs to be stored
 * */
static uint8_t saved_day = 0;
static uint8_t saved_month = 0;
static uint8_t saved_hour = 0;
static uint8_t saved_min = 0;


// Pre-fill the four entries
static void entry_load(uint8_t left, uint8_t right) {
	// Split left/right parameters into tens and units places and store them in entry[0]/entry[1]
    entry[0] = (char)('0' + left / 10);
    entry[1] = (char)('0' + left % 10);
    entry[2] = (char)('0' + right / 10);
    entry[3] = (char)('0' + right % 10);
    // curr_entry_i back at the front. Let the next input start overwriting from the first digit.
    curr_entry_i = 0;
}

// Type the entry
static void entry_type(char digit) {
	// Write a number at the curr_entry_i position
    entry[curr_entry_i] = digit;
    // a 5th digit wraps back to the front.
    curr_entry_i = (uint8_t)((curr_entry_i + 1) % ADMIN_DIGITS);
}


// Assemble the four digits into a display format [xx:xx] or [xx/xx]
static void entry_to_text(char *out) {
    out[0] = '[';
    out[1] = entry[0];
    out[2] = entry[1];

    if (screen == ADMIN_SCHED_DATE || screen == ADMIN_SET_DATE) {
    	out[3] = '/';
	} else {
		out[3] = ':';
	}

    out[4] = entry[2];
    out[5] = entry[3];
    out[6] = ']';
    out[7] = '\0';
}

// Two ASCII digit characters -> the two-digit number they spell
static uint8_t ascii_to_decimal(char tens, char units) {
    return (uint8_t)((tens - '0') * 10 + (units - '0'));
}

// Turn the 4 digits into hours/minutes
static bool entry_to_time(uint8_t *hour, uint8_t *minute) {
    uint8_t h = ascii_to_decimal(entry[0], entry[1]);
    uint8_t m = ascii_to_decimal(entry[2], entry[3]);

    // Return false if the input is invalid
    if (h > 23 || m > 59) {
        return false;
    }

    *hour = h;
    *minute = m;
    return true;
}

// Turn the 4 digits into day/month
static bool entry_to_date(uint8_t *day, uint8_t *month) {
    uint8_t d = ascii_to_decimal(entry[0], entry[1]);
    uint8_t m = ascii_to_decimal(entry[2], entry[3]);

    // Return false if the input is invalid
    if (d < 1 || d > 31 || m < 1 || m > 12) {
        return false;
    }

    *day = d;
    *month = m;
    return true;
}

// Turn the 4 digits into a duration.
static bool entry_to_duration(uint8_t *hours, uint8_t *minutes) {
    uint8_t h = ascii_to_decimal(entry[0], entry[1]);
    uint8_t m = ascii_to_decimal(entry[2], entry[3]);

    // Return false if the input is invalid
    if (h > 23U || m > 59U) {
        return false;
    }

    *hours = h;
    *minutes = m;
    return true;
}

/* Park the blinking cursor on the digit the next keypress will overwrite.
 * entry 0~3 sit at col 1, 2, 4 and 5. */
static void show_entry_cursor(void) {
    uint8_t col;

    // Col0 is taken by '[', the first digit starts at col1.
    col = 1 + curr_entry_i;

    // Col3 is taken by ':' or '/'
    if (curr_entry_i >= 2) {
        col = col + 1;
    }

    lcd_cursor_at(1, col);
}

/* Show an error and keep the typed value on screen,
 * so the admin can check the input and retype valid value */
static void show_invalid_entry(const char *err_message) {
    char field[8];
    char line2[20];

    entry_to_text(field);
    snprintf(line2, sizeof(line2), "%s retype", field);
    lcd_print(err_message, line2);
    show_entry_cursor();
}

// Redraw the screen
static void show_screen(void) {
    char field[8];
    char line1[20];
    char line2[20];

    switch (screen) {

    case ADMIN_MENU_PAGE1:
        lcd_print("Press 1-4 B:Next", "1.Schdl  2.Lock");
        lcd_cursor_hide();
        break;

    case ADMIN_MENU_PAGE2:
        lcd_print("Press 1-4 A:Back", "3.Clock  4.Exit");
        lcd_cursor_hide();
        break;

    case ADMIN_SCHED_MENU:
        lcd_print("1:View   2:Set", "A:Back");
        lcd_cursor_hide();
        break;

    case ADMIN_SCHED_VIEW: {
        uint8_t  mo, d, h, mi;
        uint16_t dur;

        if (scheduler_get_event(&mo, &d, &h, &mi, &dur)) {
            snprintf(line1, sizeof(line1), "%02u/%02u  %02u:%02u",
                     (unsigned)d, (unsigned)mo, (unsigned)h, (unsigned)mi);
            snprintf(line2, sizeof(line2), "%uh%02um  C:Cancel",
                     (unsigned)(dur / 60), (unsigned)(dur % 60));
            lcd_print(line1, line2);
        } else {
            lcd_print("No event set", "A:Back");
        }
        lcd_cursor_hide();
        break;
    }

    case ADMIN_SCHED_DATE:
        entry_to_text(field);
        snprintf(line2, sizeof(line2), "%s   B:Nxt", field);
        lcd_print("Date DD/MM", line2);
        show_entry_cursor();
        break;

    case ADMIN_SCHED_START:
        entry_to_text(field);
        snprintf(line2, sizeof(line2), "%s   B:Nxt", field);
        lcd_print("Start HH:MM", line2);
        show_entry_cursor();
        break;

    case ADMIN_SCHED_DUR:
        entry_to_text(field);
        snprintf(line2, sizeof(line2), "%s  #:Save", field);
        lcd_print("For   HH:MM", line2);
        show_entry_cursor();
        break;

    case ADMIN_SET_DATE:
        entry_to_text(field);
        snprintf(line2, sizeof(line2), "%s   B:Nxt", field);
        lcd_print("Today DD/MM", line2);
        show_entry_cursor();
        break;

    case ADMIN_SET_CLOCK:
        entry_to_text(field);
        snprintf(line2, sizeof(line2), "%s  #:Save", field);
        lcd_print("Now   HH:MM", line2);
        show_entry_cursor();
        break;

    case ADMIN_LOCK_CONFIG:
        if (fsm_get_admin_origin() == UNLOCKED) {
            lcd_print("#:Select A:Back", ">Restore NORMAL");
        } else {
            lcd_print("#:Select A:Back", ">Set OPEN");
        }
        lcd_cursor_hide();
        break;
    }
}

// public interface

/* Called by fsmcore when an admin card opens the menu. Shows a splash for
 * two seconds, then resets to the first page. */
void admin_menu_enter(void) {
    printf("Admin: menu opened\r\n");

    lcd_print("   ADMIN MODE", "    Welcome");
    HAL_Delay(ADMIN_SPLASH_MS);

    screen = ADMIN_MENU_PAGE1;
    show_screen();
}

// Called by fsmcore on every path out of the menu.
void admin_menu_exit(void) {
	// Resets the screen state
    screen = ADMIN_MENU_PAGE1;
    printf("Admin: menu closed\r\n");
}

// Handle one keypress, called by fsmcore while in ADMIN
void admin_menu_handle_key(char key) {

    switch (screen) {

    /* main menu, page 1 */
    case ADMIN_MENU_PAGE1:
        if (key == '1') {
            screen = ADMIN_SCHED_MENU;
        } else if (key == '2') {
            screen = ADMIN_LOCK_CONFIG;
        } else if (key == 'B') {
            screen = ADMIN_MENU_PAGE2;
        }
        show_screen();
        break;

    /* main menu, page 2 */
    case ADMIN_MENU_PAGE2:
        if (key == '3') {
            uint8_t month, day;
            scheduler_get_date(&month, &day);
            entry_load(day, month);
            screen = ADMIN_SET_DATE;
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

    /* schedule submenu */
    case ADMIN_SCHED_MENU:
        if (key == '1') {
            screen = ADMIN_SCHED_VIEW;
        } else if (key == '2') {
            uint8_t month, day;
            // Pre-fill with today's date
            scheduler_get_date(&month, &day);
            entry_load(day, month);
            screen = ADMIN_SCHED_DATE;
        } else if (key == 'A') {
            screen = ADMIN_MENU_PAGE1;
        }
        show_screen();
        break;

    /* view what is already scheduled */
    case ADMIN_SCHED_VIEW:
        if (key == 'C') {
            scheduler_disable();
            lcd_print("Event", "cancelled");
            HAL_Delay(ADMIN_CONFIRM_MS);
            screen = ADMIN_SCHED_MENU;
        } else if (key == 'A') {
            screen = ADMIN_SCHED_MENU;
        }
        show_screen();
        break;

    /* event: which day */
    case ADMIN_SCHED_DATE:
        if (key >= '0' && key <= '9') {
            entry_type(key);
        } else if (key == 'A') {
            screen = ADMIN_SCHED_MENU;
        } else if (key == 'B') {
            uint8_t day, month;
            // if the input date is invalid
            if (!entry_to_date(&day, &month)) {
                show_invalid_entry("Invalid date");
                return;
            }
            // store the input date before calling entry_load
            saved_day = day;
            saved_month = month;

            entry_load(ADMIN_DEFAULT_START_HOUR, ADMIN_DEFAULT_START_MIN);
            screen = ADMIN_SCHED_START;
        }
        show_screen();
        break;

    /* event: what time */
    case ADMIN_SCHED_START:
        if (key >= '0' && key <= '9') {
            entry_type(key);
        } else if (key == 'A') {
            entry_load(saved_day, saved_month);
            screen = ADMIN_SCHED_DATE;
        } else if (key == 'B') {
            uint8_t hour, minute;
            if (!entry_to_time(&hour, &minute)) {
                show_invalid_entry("Invalid time");
                return;
            }
            saved_hour = hour;
            saved_min = minute;

            entry_load(ADMIN_DEFAULT_DUR_HOUR, ADMIN_DEFAULT_DUR_MIN);
            screen = ADMIN_SCHED_DUR;
        }
        show_screen();
        break;

    /* event: what is the duration, then save */
    case ADMIN_SCHED_DUR:
        if (key >= '0' && key <= '9') {
            entry_type(key);
        } else if (key == 'A') {
            entry_load(saved_hour, saved_min);
            screen = ADMIN_SCHED_START;
        } else if (key == '#') {
            uint8_t  entered_duration_hour, entered_duration_min;
            uint16_t duration_min;
            uint16_t start_min;

            // if the length is invalid
            if (!entry_to_duration(&entered_duration_hour, &entered_duration_min)) {
                show_invalid_entry("Invalid length");
                return;
            }

            // transfer the entered duration to minutes
            duration_min = (uint16_t)(entered_duration_hour * 60 + entered_duration_min);
            start_min = (uint16_t)(saved_hour * 60 + saved_min);

            // if the duration is 0 minutes
            if (duration_min == 0) {
                show_invalid_entry("Zero length");
                return;
            }
            // Events are limited to a single day
            if (start_min + duration_min > MINUTES_PER_DAY) {
                show_invalid_entry("Ends next day");
                return;
            }

            // set the scheduled event using the input data
            scheduler_set_event(saved_month, saved_day,
                                saved_hour, saved_min, duration_min);

            lcd_print("Event saved", "");
            HAL_Delay(ADMIN_CONFIRM_MS);
            screen = ADMIN_SCHED_MENU;
        }
        show_screen();
        break;

    /* set the clock: today's date */
    case ADMIN_SET_DATE:
        if (key >= '0' && key <= '9') {
            entry_type(key);
        } else if (key == 'A') {
            screen = ADMIN_MENU_PAGE2;
        } else if (key == 'B') {
            uint8_t day, month;

            if (!entry_to_date(&day, &month)) {
                show_invalid_entry("Invalid date");
                return;
            }

            saved_day = day;
            saved_month = month;

            uint8_t hour, minute;
            scheduler_get_time(&hour, &minute);
            entry_load(hour, minute);
            screen = ADMIN_SET_CLOCK;
        }
        show_screen();
        break;

    /* set the clock: current time, then save both */
    case ADMIN_SET_CLOCK:
        if (key >= '0' && key <= '9') {
            entry_type(key);
        } else if (key == 'A') {
            entry_load(saved_day, saved_month);
            screen = ADMIN_SET_DATE;
        } else if (key == '#') {

            uint8_t hour, minute;
            if (!entry_to_time(&hour, &minute)) {
                show_invalid_entry("Invalid time");
                return;
            }

            // set the scheduler
            scheduler_set_date(saved_month, saved_day);
            scheduler_set_clock(hour, minute);

            lcd_print("Clock updated", "");
            HAL_Delay(ADMIN_CONFIRM_MS);
            screen = ADMIN_MENU_PAGE2;
        }
        show_screen();
        break;

    /* lock configuration */
    case ADMIN_LOCK_CONFIG:
        if (key == '#') {
        	// if the door is currently unlocked, restore the NORMAL mode
            if (fsm_get_admin_origin() == UNLOCKED) {
                lcd_print("Restoring", "NORMAL mode");
                HAL_Delay(ADMIN_CONFIRM_MS);
                fsm_dispatch(EVT_ADMIN_SET_NORMAL);
            } else {
            // if the door is currently locked, turn to the open mode
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

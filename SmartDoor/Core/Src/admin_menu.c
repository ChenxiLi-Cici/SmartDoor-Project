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

// The 4 digits shown in the [xx:xx] field
static char    entry[ADMIN_DIGITS];
// Which entry (0 to 3) will the next key be written into
static uint8_t cursor = 0;

/* Fields held aside while the later screens of a multi-step entry are being
 * typed. Shared between the event flow and the set-clock flow, which can
 * never be active at the same time. */
static uint8_t pending_day   = 0;
static uint8_t pending_month = 0;
static uint8_t pending_hour  = 0;
static uint8_t pending_min   = 0;

/* ---------------- entry helpers ---------------- */

/* Pre-fill the two 2-digit fields. Depending on the screen these hold
 * hour/minute, day/month, or the hours/minutes of a duration. */
static void entry_load(uint8_t left, uint8_t right) {
    entry[0] = (char)('0' + left / 10);
    entry[1] = (char)('0' + left % 10);
    entry[2] = (char)('0' + right / 10);
    entry[3] = (char)('0' + right % 10);
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

// Assemble the four digits into a display format [xx:xx]
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

// Turn the 4 digits into day/month; false if not a plausible date.
static bool entry_to_date(uint8_t *day, uint8_t *month) {
    uint8_t d = (uint8_t)((entry[0] - '0') * 10 + (entry[1] - '0'));
    uint8_t m = (uint8_t)((entry[2] - '0') * 10 + (entry[3] - '0'));

    if (d < 1U || d > 31U || m < 1U || m > 12U) {
        return false;
    }

    *day   = d;
    *month = m;
    return true;
}

/* Turn the 4 digits into a duration. Same range as a clock reading, because
 * an event cannot outlast the day it starts on anyway. */
static bool entry_to_duration(uint8_t *hours, uint8_t *minutes) {
    uint8_t h = (uint8_t)((entry[0] - '0') * 10 + (entry[1] - '0'));
    uint8_t m = (uint8_t)((entry[2] - '0') * 10 + (entry[3] - '0'));

    if (h > 23U || m > 59U) {
        return false;
    }

    *hours   = h;
    *minutes = m;
    return true;
}

/* Park the blinking cursor on the digit the next keypress will overwrite.
 * The field is drawn as "[xx:xx]" at the start of line 2:
 *
 *   column  0  1  2  3  4  5  6
 *   char    [  x  x  :  x  x  ]
 *
 * so entry[0..3] sit at columns 1, 2, 4 and 5. */
static void show_entry_cursor(void) {
    uint8_t col;

    // Column 0 is taken by '[', so the first digit starts at column 1.
    col = 1 + cursor;

    /* The right-hand pair sits one column further right, because the colon
     * takes column 3. */
    if (cursor >= 2) {
        col = col + 1;
    }

    lcd_cursor_at(1, col);
}

/* Show an error and keep the typed value on screen, so the admin can see
 * what went wrong and carry on typing over it. */
static void show_invalid_entry(const char *reason) {
    char field[8];
    char line2[20];

    entry_to_text(field);
    snprintf(line2, sizeof(line2), "%s retype", field);
    lcd_print(reason, line2);
    show_entry_cursor();
}

// Redraw whichever screen is currently active.
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
        lcd_print("Date DD:MM", line2);
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
        lcd_print("Today DD:MM", line2);
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

/* Called by fsmcore on every path out of the menu. Only resets the screen
 * state; the LCD is left alone because the state we are leaving to will
 * draw its own content. */
void admin_menu_exit(void) {
    screen = ADMIN_MENU_PAGE1;
    printf("Admin: menu closed\r\n");
}

/* Handle one keypress, dispatched by fsmcore while the door is in ADMIN.
 * The outer switch picks the current screen, and each case decides what the
 * key means there. Note '*' never arrives: fsmcore intercepts it as "leave
 * the menu", so the menu only uses 0-9, A-D and '#'. */
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
            uint8_t mo, d;
            scheduler_get_date(&mo, &d);
            entry_load(d, mo);
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
            uint8_t mo, d;
            // Pre-fill with today's date
            scheduler_get_date(&mo, &d);
            entry_load(d, mo);
            screen = ADMIN_SCHED_DATE;
        } else if (key == 'A') {
            screen = ADMIN_MENU_PAGE1;
        }
        show_screen();
        break;

    /* show what is already scheduled */
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
            uint8_t d, mo;
            if (!entry_to_date(&d, &mo)) {
                show_invalid_entry("Invalid date");
                return;
            }
            pending_day   = d;
            pending_month = mo;

            entry_load(ADMIN_DEFAULT_START_HOUR, ADMIN_DEFAULT_START_MIN);
            screen = ADMIN_SCHED_START;
        }
        show_screen();
        break;

    /* event: what time it starts */
    case ADMIN_SCHED_START:
        if (key >= '0' && key <= '9') {
            entry_type(key);
        } else if (key == 'A') {
            entry_load(pending_day, pending_month);
            screen = ADMIN_SCHED_DATE;
        } else if (key == 'B') {
            uint8_t h, mi;
            if (!entry_to_time(&h, &mi)) {
                show_invalid_entry("Invalid time");
                return;
            }
            pending_hour = h;
            pending_min  = mi;

            entry_load(ADMIN_DEFAULT_DUR_HOUR, ADMIN_DEFAULT_DUR_MIN);
            screen = ADMIN_SCHED_DUR;
        }
        show_screen();
        break;

    /* event: how long it lasts, then save */
    case ADMIN_SCHED_DUR:
        if (key >= '0' && key <= '9') {
            entry_type(key);
        } else if (key == 'A') {
            entry_load(pending_hour, pending_min);
            screen = ADMIN_SCHED_START;
        } else if (key == '#') {
            uint8_t  dh, dm;
            uint16_t duration_min;
            uint16_t start_min;

            if (!entry_to_duration(&dh, &dm)) {
                show_invalid_entry("Invalid length");
                return;
            }

            duration_min = (uint16_t)(dh * 60 + dm);
            start_min    = (uint16_t)(pending_hour * 60 + pending_min);

            if (duration_min == 0U) {
                show_invalid_entry("Zero length");
                return;
            }
            // Events are limited to a single day
            if (start_min + duration_min > MINUTES_PER_DAY) {
                show_invalid_entry("Ends next day");
                return;
            }

            scheduler_set_event(pending_month, pending_day,
                                pending_hour, pending_min, duration_min);
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
            uint8_t d, mo;
            if (!entry_to_date(&d, &mo)) {
                show_invalid_entry("Invalid date");
                return;
            }
            pending_day   = d;
            pending_month = mo;

            uint8_t h, mi;
            scheduler_get_time(&h, &mi);
            entry_load(h, mi);
            screen = ADMIN_SET_CLOCK;
        }
        show_screen();
        break;

    /* set the clock: current time, then save both */
    case ADMIN_SET_CLOCK:
        if (key >= '0' && key <= '9') {
            entry_type(key);
        } else if (key == 'A') {
            entry_load(pending_day, pending_month);
            screen = ADMIN_SET_DATE;
        } else if (key == '#') {
            uint8_t h, mi;
            if (!entry_to_time(&h, &mi)) {
                show_invalid_entry("Invalid time");
                return;
            }

            scheduler_set_date(pending_month, pending_day);
            scheduler_set_clock(h, mi);

            lcd_print("Clock updated", "");
            HAL_Delay(ADMIN_CONFIRM_MS);
            screen = ADMIN_MENU_PAGE2;
        }
        show_screen();
        break;

    /* lock configuration */
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

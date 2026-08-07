#include "fsm.h"
#include "output.h"
#include "admin_menu.h" 
#include "ldr.h"
#include <stdbool.h>

#define ALERT_DURATION_MS 10000 //alarm 10s
#define BLINK_INTERVAL_MS 200 //200ms
#define INVALID_CARD_DURATION_MS 2000//2s
#define CLOSING_TRAVEL_MS 2500 //door closing time
#define PASSAGE_WAIT_TIMEOUT_MS 10000U // waiting for user passage

typedef enum {
	NONE,
	FSM_TIMEOUT,
	IDLE_REVERT
} TimerPurpose_t;

typedef enum
{
	ENTRY_AUTH_NONE,
	ENTRY_AUTH_AVAILABLE,
	ENTRY_AUTH_USED
} EntryAuthState_t;

static char pressed_key = 0;

static DoorState_t current_state;
static CardType_t last_card_type = CARD_NONE;
static Direction_t passage_direction = DIR_ENTRY;

// the variable that record which state the admin menu was opened from
static DoorState_t admin_return_state = IDLE;

static EntryAuthState_t entry_auth_state = ENTRY_AUTH_NONE;
static bool authorised_entry_queued = false;

static volatile uint32_t fsm_timer_ms = 0;
static volatile uint8_t fsm_timer_expired = 0;
static volatile TimerPurpose_t fsm_timer_purpose = NONE;

static void start_fsm_timer(uint32_t duration_ms, TimerPurpose_t purpose) {
	fsm_timer_purpose = purpose;
	fsm_timer_ms      = duration_ms;
	fsm_timer_expired = 0;
}

static void reset_fsm_timer(void) {
	fsm_timer_ms      = 0;
	fsm_timer_purpose = NONE;
	fsm_timer_expired = 0;
}

const char *fsm_state_name(DoorState_t state) {
	switch (state) {
		case IDLE:
			return "IDLE";
		case AUTHORISED:
			return "AUTHORISED";
		case PASSAGE:
			return "PASSAGE";
		case ALERT:
			return "ALERT";
		case CLOSING:
			return "CLOSING";
		case ADMIN:
			return "ADMIN";
		case UNLOCKED:
			return "UNLOCKED";
		default:
			return "UNKNOWN";
	}
}

static void enter_idle(void) {
	reset_fsm_timer();
	entry_auth_state = ENTRY_AUTH_NONE;
	authorised_entry_queued = false;
	ldr_unlock_direction();

	led_off();
	lcd_print("Smart Door", "Scan card");
}

static void enter_admin(void) {
	admin_menu_enter();
}


// Leaving the admin menu.
static void leave_admin(void) {
	admin_menu_exit();
	//  If the door was being held open when the menu was opened
	if (admin_return_state == UNLOCKED) {
		/* Restore the state but do not call enter_unlocked()
		   because the gate has turned 90 degrees */
		current_state = UNLOCKED;
		lcd_print("Event mode", "Door open");
	} else {
		current_state = IDLE;
		enter_idle();
	}
}

static void enter_authorised(void) {
	passage_direction = DIR_ENTRY;
	entry_auth_state = ENTRY_AUTH_AVAILABLE;
	authorised_entry_queued = false;
	ldr_lock_direction(DIR_ENTRY);

	lcd_print("Access granted", "Approach door");
	led_signal_authorised();
	start_fsm_timer(PASSAGE_WAIT_TIMEOUT_MS, FSM_TIMEOUT);
}

static void enter_passage(void) {
	lcd_print("Please pass", "");
}

static void start_authorised_entry_passage(void)
{
	reset_fsm_timer();
	passage_direction = DIR_ENTRY;
	ldr_lock_direction(DIR_ENTRY);
	current_state = PASSAGE;
	motor_open(DIR_ENTRY);
	enter_passage();
}

static void enter_exit_passage(void)
{
	passage_direction = DIR_EXIT;
	entry_auth_state = ENTRY_AUTH_NONE;
	authorised_entry_queued = false;
	ldr_lock_direction(DIR_EXIT);

	lcd_print("Exit", "Door opening");
	motor_open(passage_direction);
}

static void reopen_for_safety(Direction_t direction)
{
	reset_fsm_timer();
	passage_direction = direction;
	ldr_lock_direction(direction);
	motor_open(direction);
	start_fsm_timer(PASSAGE_WAIT_TIMEOUT_MS, FSM_TIMEOUT);
}

static void start_simultaneous_request(void)
{
	reset_fsm_timer();

	// Preserve an unused entry authorization so that the authorized entrant can pass after the exit.
	authorised_entry_queued = (entry_auth_state == ENTRY_AUTH_AVAILABLE);

	//Exit has priority because it does not require authorization and people inside must not be trapped.
	passage_direction = DIR_EXIT;
	ldr_lock_direction(DIR_EXIT);
	motor_open(DIR_EXIT);
}

static void start_queued_entry(void)
{
	authorised_entry_queued = false;
	passage_direction = DIR_ENTRY;
	ldr_lock_direction(DIR_ENTRY);

	// The simultaneous sequence has already reset after both LDRs became clear.
	// Reopen the door for the queued authorized entrant.
	motor_open(DIR_ENTRY);
	start_fsm_timer(PASSAGE_WAIT_TIMEOUT_MS, FSM_TIMEOUT);
}

static void enter_alert(void) {
	//Keep monitoring the LDRs and keep the door open while the suspected tailgater is in transit.
	ldr_lock_direction(passage_direction);
	motor_open(passage_direction);

	lcd_print("!! ALERT !!", "Tailgating");
	led_start_blink(ALERT_DURATION_MS, BLINK_INTERVAL_MS);
	buzzer_alert(ALERT_DURATION_MS);
	start_fsm_timer(ALERT_DURATION_MS, FSM_TIMEOUT);
}

static void enter_closing(void) {
	lcd_print("Closing door", "");
	led_off();
	motor_close();
	start_fsm_timer(CLOSING_TRAVEL_MS, FSM_TIMEOUT);
}

// Start closing only when both LDRs are clear.
// If the passage is blocked, wait one second before checking again.
static void request_close(void)
{
	if (!ldr_path_is_clear()) {
		current_state = PASSAGE;
		lcd_print("Path blocked", "Waiting to close");
		start_fsm_timer(1000U, FSM_TIMEOUT);
		return;
	}

	current_state = CLOSING;
	enter_closing();
}

static void enter_unlocked(void) {
	ldr_unlock_direction();
	lcd_print("Event mode", "Door open");
	motor_open(DIR_ENTRY);
}

void fsm_init(void) {
	current_state  = IDLE;
	last_card_type = CARD_NONE;
	pressed_key = 0;
	reset_fsm_timer();
	enter_idle();
}
void fsm_set_card_type(CardType_t type) {
	last_card_type = type;
}

void fsm_set_key(char key) {
	pressed_key = key;
}

DoorState_t fsm_get_state(void) {
	return current_state;
}

DoorState_t fsm_get_admin_origin(void) {
	return admin_return_state;
}


void fsm_dispatch(Event_t event) {
	switch (current_state) {

	case IDLE:
		if (event == EVT_CARD_SCANNED) {
			switch (last_card_type) {
				case CARD_NORMAL:
					current_state = AUTHORISED;
					enter_authorised();

					// If the person was already standing at LDR1 when the card
					// was read, the required entry request is already present.
					if (ldr_entry_sensor_is_blocked()) {
						start_authorised_entry_passage();
					}
					break;
				case CARD_ADMIN:
					admin_return_state = IDLE;
					current_state = ADMIN;
					enter_admin();
					break;
				case CARD_INVALID:
					lcd_print("Invalid card", "");
					led_start_blink(INVALID_CARD_DURATION_MS, BLINK_INTERVAL_MS);
					buzzer_alert(INVALID_CARD_DURATION_MS);
					start_fsm_timer(INVALID_CARD_DURATION_MS, IDLE_REVERT);
					break;
				default:
					break;
			}
		}
		else if (event == EVT_EXIT_REQUEST) {
			current_state = PASSAGE;
			enter_exit_passage();
		}
		else if (event == EVT_BOTH_LDRS_BLOCKED) {
			current_state = PASSAGE;
			start_simultaneous_request();
		}
		else if (event == EVT_SCHEDULE_START) {
			current_state = UNLOCKED;
			enter_unlocked();
		}
		break;

	case PASSAGE:
		if ((event == EVT_ENTRY_CONFIRMED) && (passage_direction == DIR_ENTRY)) {
			//The first confirmed entrant consumes the one available authorization.
			if (entry_auth_state == ENTRY_AUTH_AVAILABLE) {
				entry_auth_state = ENTRY_AUTH_USED;
			}
			//Another entry was confirmed without an available authorization: tailgating.
			else {
				passage_direction = DIR_ENTRY;
				current_state = ALERT;
				enter_alert();
			}
		}
		else if ((event == EVT_ENTRY_REQUEST) && (passage_direction == DIR_ENTRY)) {
			// The authorized entrant has started moving.
			// LDR sequence timing now controls the passage.
			reset_fsm_timer();
		}

		else if ((event == EVT_EXIT_REQUEST) && (passage_direction == DIR_EXIT)) {
			// The owner has started moving; the LDR sequence now controls
			// passage completion.
			reset_fsm_timer();
		}

		else if (event == EVT_BOTH_LDRS_BLOCKED) {
			// A passage already has an owner. Keep its direction and make the
			// opposite side wait until this passage and closing cycle finish.
		}

		else if ((event == EVT_PASSAGE_DONE) || (event == EVT_PASSAGE_CANCELLED)) {
			if (authorised_entry_queued && (entry_auth_state == ENTRY_AUTH_AVAILABLE)) {
				start_queued_entry();
			}
			else {
				request_close();
			}
		}

		else if (event == EVT_TIMEOUT) {
			request_close();
		}

		else if ((event == EVT_TAILGATE_DETECTED) && (passage_direction == DIR_ENTRY)) {
					passage_direction = DIR_ENTRY;
					current_state = ALERT;
					enter_alert();
		}
		break;

	case ALERT:
		if (event == EVT_CARD_SCANNED &&
			last_card_type == CARD_ADMIN) {
			buzzer_off();
			led_off();

			// Stop the alert, but close only after the passage is clear.
			request_close();
		}
		else if (event == EVT_TIMEOUT) {
			request_close();
		}
		break;

	case CLOSING:
		if (event == EVT_BOTH_LDRS_BLOCKED) {
			current_state = PASSAGE;
			start_simultaneous_request();
		}
		else if (event == EVT_ENTRY_REQUEST) {
			current_state = PASSAGE;
			reopen_for_safety(DIR_ENTRY);
		}
		else if (event == EVT_EXIT_REQUEST) {
			current_state = PASSAGE;
			reopen_for_safety(DIR_EXIT);
		}
		else if (event == EVT_TIMEOUT) {
			current_state = IDLE;
			enter_idle();
		}
		break;

	case ADMIN:
		if (event == EVT_ADMIN_EXIT) {
			leave_admin();
		}
		// The administrator forced the door open.
		else if (event == EVT_ADMIN_SET_OPEN) {
			// clear the menu
			admin_menu_exit();
			current_state = UNLOCKED;
			enter_unlocked();
		}
		// The administrator selected "Restore Normal".
		else if (event == EVT_ADMIN_SET_NORMAL) {
			admin_menu_exit();
			request_close();
		}
		// handle key
		else if (event == EVT_KEYPAD_KEY) {
			if (pressed_key == '*') {
				leave_admin();
			} else {
				admin_menu_handle_key(pressed_key);
			}
		}
		break;

	case UNLOCKED:
		if (event == EVT_SCHEDULE_END) {
			request_close();
		}
		// Enable the administrator to swipe the card to enter the menu even when the door is forcibly opened.
		else if (event == EVT_CARD_SCANNED && last_card_type == CARD_ADMIN) {
			admin_return_state = UNLOCKED;
			current_state = ADMIN;
			enter_admin();
		}
		break;

	case AUTHORISED:
		if (event == EVT_ENTRY_REQUEST) {
			start_authorised_entry_passage();
		}
		else if (event == EVT_ENTRY_CONFIRMED) {
			// This can occur when LDR1 was already active as the card was read.
			// Open the door and consume the one available authorisation.
			start_authorised_entry_passage();
			entry_auth_state = ENTRY_AUTH_USED;
		}
		else if (event == EVT_TIMEOUT) {
			current_state = IDLE;
			enter_idle();
		}
		break;

	}
}

void fsm_tick_1ms(void) {
	if (fsm_timer_ms > 0) {
		fsm_timer_ms--;
		if (fsm_timer_ms == 0) {
			fsm_timer_expired = 1;
		}
	}
}

void fsm_poll(void) {
	// Safety fallback: if both LDRs are blocked together, there may be no direction event.
	// Reopen using the most recent passage direction.
	if ((current_state == CLOSING) && !ldr_path_is_clear()) {
		current_state = PASSAGE;
		reopen_for_safety(passage_direction);
		return;
	}

	if (fsm_timer_expired) {
		fsm_timer_expired = 0;
		TimerPurpose_t purpose = fsm_timer_purpose;
		fsm_timer_purpose = NONE;
		if (purpose == FSM_TIMEOUT) {
			fsm_dispatch(EVT_TIMEOUT);
		} else if ((purpose == IDLE_REVERT) && (current_state == IDLE)) {
			enter_idle();
		}
	}
}

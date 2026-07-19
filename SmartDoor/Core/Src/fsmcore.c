#include "fsm.h"
#include "output.h"
#include "admin_menu.h" 
#include "input.h"

#define ALERT_DURATION_MS 10000 //alarm 10s
#define BLINK_INTERVAL_MS 200 //200ms
#define INVALID_CARD_DURATION_MS 2000//2s
// to be comfirm
#define CLOSING_TRAVEL_MS 3000 //door closing time

static DoorState_t current_state;
static CardType_t last_card_type = CARD_NONE;
static char pressed_key = 0;

typedef enum {
    NONE,
    FSM_TIMEOUT,
    IDLE_REVERT
} TimerPurpose_t;

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
    //ldr_arm(false);
    led_off();
    //lcd_print("Smart Door", "Scan card");
}

static void enter_admin(void) {
    admin_menu_enter();
}

static void enter_authorised(void) {
    //lcd_print("Access granted", "Please enter");
    led_signal_authorised();
    motor_open(DIR_ENTRY);
    //ldr_arm(true);
}

static void enter_passage(void) {
    //lcd_print("Please pass", "");
}

static void enter_alert(void) {
    //ldr_arm(false);
    //lcd_print("!! ALERT !!", "Tailgating");
    led_start_blink(ALERT_DURATION_MS, BLINK_INTERVAL_MS);
    buzzer_alert(ALERT_DURATION_MS);
    start_fsm_timer(ALERT_DURATION_MS, FSM_TIMEOUT);
}

static void enter_closing(void) {
    //ldr_arm(false);
    //lcd_print("Closing door", "");
    led_off();
    motor_close();
    start_fsm_timer(CLOSING_TRAVEL_MS, FSM_TIMEOUT);
}

static void enter_unlocked(void) {
    //lcd_print("Event mode", "Door open");
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


void fsm_dispatch(Event_t event) {
    switch (current_state) {

    case IDLE:
        if (event == EVT_CARD_SCANNED) {
            switch (last_card_type) {
                case CARD_NORMAL:
                    current_state = AUTHORISED;
                    enter_authorised();
                    current_state = PASSAGE;
                    enter_passage();
                    break;
                case CARD_ADMIN:
                    current_state = ADMIN;
                    enter_admin();
                    break;
                case CARD_INVALID:
                    //lcd_print("Invalid card", "");
                    led_start_blink(INVALID_CARD_DURATION_MS, BLINK_INTERVAL_MS);
                    start_fsm_timer(INVALID_CARD_DURATION_MS, IDLE_REVERT);
                    break;
                default:
                    break;
            }
        } else if (event == EVT_SCHEDULE_START) {
            current_state = UNLOCKED;
            enter_unlocked();
        }
        break;

    case PASSAGE:
        if (event == EVT_PASSAGE_DONE) {
            current_state = CLOSING;
            enter_closing();
        } else if (event == EVT_TAILGATE_DETECTED) {
            current_state = ALERT;
            enter_alert();
        }
        break;

    case ALERT:
        if (event == EVT_TIMEOUT) {
            current_state = CLOSING;
            enter_closing();
        }
        break;

    case CLOSING:
        if (event == EVT_TIMEOUT) {
            current_state = IDLE;
            enter_idle();
        }
        break;
        
    case ADMIN:
        if (event == EVT_ADMIN_EXIT) {
            admin_menu_exit();
            current_state = IDLE;
            enter_idle();
        } else if (event == EVT_KEYPAD_KEY) {
            if (pressed_key == '*') {
                admin_menu_exit();
                current_state = IDLE;
                enter_idle();
            } else {
                admin_menu_handle_key(pressed_key);
            }
        }
        break;
        
    case UNLOCKED:
        if (event == EVT_SCHEDULE_END) {
            motor_close();
            current_state = IDLE;
            enter_idle();
        }
        break;

    case AUTHORISED:
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
    if (fsm_timer_expired) {
        fsm_timer_expired = 0;
        TimerPurpose_t purpose = fsm_timer_purpose;
        fsm_timer_purpose = NONE;
        if (purpose == FSM_TIMEOUT) {
            fsm_dispatch(EVT_TIMEOUT);
        } else if (purpose == IDLE_REVERT) {
            //lcd_print("Smart Door", "Scan card...");
        }
    }
}
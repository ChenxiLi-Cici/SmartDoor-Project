
#ifndef FSM_H
#define FSM_H

// The direction of the motor
typedef enum {
    DIR_ENTRY,
    DIR_EXIT
} Direction_t;


typedef enum {
    IDLE,
    AUTHORISED,
    PASSAGE,
    ALERT,
    CLOSING,
    ADMIN,
    UNLOCKED
} DoorState_t;

typedef enum {
    CARD_INVALID,
    CARD_NORMAL,
    CARD_ADMIN,
    CARD_NONE
} CardType_t;

typedef enum {
    EVT_NONE,
    EVT_CARD_SCANNED,
    EVT_LDR_FRONT,
    EVT_LDR_REAR,
    EVT_ENTRY_REQUEST,
    EVT_EXIT_REQUEST,
	EVT_BOTH_LDRS_BLOCKED,
	EVT_ENTRY_CONFIRMED,
	EVT_EXIT_CONFIRMED,
    EVT_PASSAGE_DONE,
	EVT_PASSAGE_CANCELLED,
    EVT_TAILGATE_DETECTED,
    EVT_TIMEOUT,
    EVT_KEYPAD_KEY,
    EVT_ADMIN_EXIT,
    EVT_SCHEDULE_START,
    EVT_SCHEDULE_END
} Event_t;


void fsm_init(void);
void fsm_dispatch(Event_t evt);
DoorState_t fsm_get_state(void);
const char *fsm_state_name(DoorState_t state);
void fsm_set_card_type(CardType_t type);
void fsm_set_key(char key);
void fsm_tick_1ms(void);
void fsm_poll(void);

#endif


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
    CARD_NONE,
    CARD_INVALID,
    CARD_NORMAL,
    CARD_ADMIN
} CardType_t;

typedef enum {
    EVT_NONE,
    EVT_CARD_SCANNED,
    EVT_LDR_FRONT,
    EVT_LDR_REAR,
    EVT_PASSAGE_DONE,
    EVT_TAILGATE_DETECTED,
    EVT_TIMEOUT,
    EVT_KEYPAD_KEY,
    EVT_ADMIN_EXIT,
    EVT_SCHEDULE_START,
    EVT_SCHEDULE_END
} Event_t;

#endif

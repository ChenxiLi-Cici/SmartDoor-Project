#include "fsm.h"
#include "output.h"
#include "admin_menu.h"

#define ALERT_DURATION_MS 10000 //alarm 10s
#define BLINK_INTERVAL_MS 200 //200ms
#define INVALID_CARD_DURATION_MS 2000//2s
// to be comfirm
#define CLOSING_TRAVEL_MS 3000 //door closing time

static DoorState_t current_state;
static CardType_t last_card_type = CARD_NONE;
static char last_key = 0;

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

static void cancel_fsm_timer(void) {
    fsm_timer_ms      = 0;
    fsm_timer_purpose = TIMER_PURPOSE_NONE;
    fsm_timer_expired = 0;
}
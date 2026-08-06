#ifndef INPUT_H
#define INPUT_H

#include <stdbool.h>
#include "main.h"
#include "fsm.h"

// Reset the NFC card latch when the input module is initialised.
void sensors_init(void);

// Poll the PN532 once and return the detected card type.
// CARD_NONE is returned when no new card event is available.
CardType_t nfc_poll_card(void);

#endif // INPUT_H

#ifndef INPUT_H
#define INPUT_H

#include <stdbool.h>
#include "main.h"
#include "fsm.h"

/* Initialize all input-module software state. */
void sensors_init(void);

/* Read NFC once and classify the detected card. */
CardType_t nfc_poll_card(void);

#endif /* INPUT_H */

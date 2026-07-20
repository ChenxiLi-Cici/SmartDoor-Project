#ifndef INPUT_H
#define INPUT_H

#include "main.h"
#include "fsm.h"

/* Initialise all input-module software state. */
void sensors_init(void);

/* Read NFC once and classify the detected card. */
CardType_t nfc_poll_card(void);

/* Enable or disable LDR passage detection. */
void ldr_arm(bool armed);

/* Poll both LDRs and return a completed sensor event. */
Event_t ldr_poll(void);

#endif /* INPUT_H */
#include "input.h"
#include "main.h"
#include <stdio.h>
#include <string.h>

// The low-level PN532 function is currently implemented in main.c.
// uid points to the buffer where the UID bytes will be written.
// uidLen points to the variable where the received UID length will be written.
extern int PN532_Get_UID(uint8_t *uid, uint8_t *uidLen);

// Store the UID values of the two cards that are allowed by the system.
// The UID length is checked before the bytes are compared.
static const uint8_t normal_card_uid[] = {0x9D, 0x9E, 0x29, 0x07};
static const uint8_t admin_card_uid[] = {0x1C, 0xEE, 0x0A, 0x07};

// Remember whether a card has already triggered an event.
// This prevents one card from being read repeatedly while it stays on the reader.
static bool card_present_swipe = false;

// Reset the NFC software state when the input module is initialised.
void sensors_init(void)
{
    card_present_swipe = false;
}

// Poll the PN532 once and classify the card as normal, admin, invalid or none.
CardType_t nfc_poll_card(void)
{
    // PN532 supports UID values up to 7 bytes, so a 7-byte buffer is provided.
    uint8_t uid[7] = {0};

    // PN532_Get_UID writes the actual number of received UID bytes into uid_len.
    uint8_t uid_len = 0;

    // A failed read means that no card is currently detected.
    // Clear the latch so that a later card can create a new event.
    if (!PN532_Get_UID(uid, &uid_len)) {
        card_present_swipe = false;
        return CARD_NONE;
    }

    // Ignore the card if the current card has already been reported.
    if (card_present_swipe) {
        return CARD_NONE;
    }

    // Latch the card before classifying it so this card can trigger only once.
    card_present_swipe = true;

    // Print every UID byte in hexadecimal form for testing and card registration.
    printf("NFC UID:");

    for (uint8_t i = 0; i < uid_len; i++) {
        printf(" %02X", uid[i]);
    }

    printf("\r\n");

    // First confirm the UID length, then compare every UID byte.
    // Matching the normal UID gives one normal entry authorisation.
    if (uid_len == sizeof(normal_card_uid) && memcmp(uid, normal_card_uid, sizeof(normal_card_uid)) == 0) {
		printf("Card type: NORMAL\r\n");
		return CARD_NORMAL;
    }

    // Matching the admin UID allows the FSM to open the admin menu.
    if (uid_len == sizeof(admin_card_uid) && memcmp(uid, admin_card_uid, sizeof(admin_card_uid)) == 0) {
        printf("Card type: ADMIN\r\n");
        return CARD_ADMIN;
    }

    // A UID was received, but it did not match either registered card.
    printf("Card type: INVALID\r\n");
    return CARD_INVALID;
}

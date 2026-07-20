#include "input.h"
#include "main.h"

static bool ldr_is_armed = false;

void sensors_init(void)
{
    ldr_is_armed = false;
}

CardType_t nfc_poll_card(void)
{
    /* NFC implementation will be added next. */
    return CARD_NONE;
}

void ldr_arm(bool armed)
{
    ldr_is_armed = armed;

    if (armed)
    {
        /*
         * Reset LDR counters, edge-detection state
         * and passage timeout here later.
         */
    }
}

Event_t ldr_poll(void)
{
    if (!ldr_is_armed)
    {
        return EVT_NONE;
    }

    /* ADC reading and passage logic will be added next. */
    return EVT_NONE;
}
#include "output.h"
#include <stdio.h>

//  timer variables

// How many ms will the red light continue to flash in total
volatile uint32_t led_blink_timeout = 0;
// A fixed interval of flashing
volatile uint32_t led_blink_interval = 0;
// How many ms are there until the next flip
volatile uint32_t led_blink_counter = 0;
// How many more ms does the buzzer need to sound
volatile uint32_t buzzer_timeout = 0;

void motor_open(Direction_t dir) {
    if (dir == DIR_ENTRY) {
        printf("Motor: Starting OPEN sequence for ENTRY.\r\n");
    } else {
        printf("Motor: Starting OPEN sequence for EXIT.\r\n");
    }
}

void motor_close(void) {
    printf("Motor: Starting CLOSE sequence.\r\n");
}

void led_signal_authorised(void) {
    printf("LED: Green light ON.\r\n");
}

void led_off(void) {
    printf("LED: LEDs OFF.\r\n");
    led_blink_timeout = 0;
}

void led_start_blink(uint32_t duration_ms, uint32_t interval_ms) {
    printf("LED: Start Red blink for %dms, interval %dms.\r\n", duration_ms, interval_ms);
    led_blink_timeout = duration_ms;
    led_blink_interval = interval_ms;
    led_blink_counter = interval_ms;
}

void buzzer_alert(uint32_t duration_ms) {
    printf("Buzzer: Alert active for %dms.\r\n", duration_ms);
    buzzer_timeout = duration_ms;
}

void lcd_print(const char *line1, const char *line2) {
    printf("LCD: LCD_PRINT\n");

}

void io_actuators_process(void) {
    // Non-blocking LED blink polling
    if (led_blink_timeout > 0) {
        if (led_blink_counter == 0) {
            printf("LED: Toggling Red LED.\r\n");
            // Reset the blink counter after toggling the LED.
            led_blink_counter = led_blink_interval;
        }
    }

    // when buzzer time expires
    if (buzzer_timeout == 0) {

    }
}

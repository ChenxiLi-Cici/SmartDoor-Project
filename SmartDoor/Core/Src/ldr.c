#include "ldr.h"
#include "main.h"
#include <stdio.h>

// A low ADC value means that the light path is blocked.
// A high ADC value means that the light path is clear.
// Values between the two thresholds keep the previous state.
#define LDR1_BLOCK_THRESHOLD 900U
#define LDR1_CLEAR_THRESHOLD 1200U
#define LDR2_BLOCK_THRESHOLD 900U
#define LDR2_CLEAR_THRESHOLD 1200U

// A complete direction must to be finish in five second, or clear current state record.
#define LDR_SEQUENCE_TIMEOUT_MS 5000U

// A new measured state must remain stable for 50 ms.
#define LDR_DEBOUNCE_MS 50U

// This state machine records the order in which the two LDRs are blocked.
// LDR1 followed by LDR2 represents entry.
// LDR2 followed by LDR1 represents exit.
typedef enum
{
	// Initial state, no passage is currently being detect.
	LDR_SEQUENCE_IDLE,

	// Entry: LDR1 BLOCK -> CLEAR, followed by LDR2 BLOCK -> CLEAR.
	LDR_SEQUENCE_ENTRY_WAIT_LDR1_CLEAR,
	LDR_SEQUENCE_ENTRY_WAIT_LDR2_BLOCK,
	LDR_SEQUENCE_ENTRY_WAIT_LDR2_CLEAR,

	// Exit: LDR2 BLOCK -> CLEAR, followed by LDR1 BLOCK -> CLEAR.
	LDR_SEQUENCE_EXIT_WAIT_LDR2_CLEAR,
	LDR_SEQUENCE_EXIT_WAIT_LDR1_BLOCK,
	LDR_SEQUENCE_EXIT_WAIT_LDR1_CLEAR,

	// Both two LDR blocked by people approaching from opposite slides.
	LDR_SEQUENCE_SIMULTANEOUS_WAIT_CLEAR,

	// Detect and report a tailgating. Wait until the passage clear.
	LDR_SEQUENCE_TAILGATE_WAIT_CLEAR
} ldrSequenceState_t;

extern ADC_HandleTypeDef hadc2;  // LDR1 is connected to PC4, ADC2 Channel 5.
extern ADC_HandleTypeDef hadc3;  // LDR2 is connected to PB1, ADC3 Channel 1.

// Store the last print time so that raw ADC values are not printed every loop.
static uint32_t last_ldr_print_ms = 0;

// The previous stable states are used for edge detection.
static ldrState_t previous_ldr1_state = LDR_CLEAR;
static ldrState_t previous_ldr2_state = LDR_CLEAR;

// The latest stable states are used by ldr_path_is_clear().
static ldrState_t latest_ldr1_state = LDR_CLEAR;
static ldrState_t latest_ldr2_state = LDR_CLEAR;

// A candidate state is a possible new state that is still being debounced.
// It becomes stable only after it remains unchanged for LDR_DEBOUNCE_MS.
static ldrState_t ldr1_candidate_state = LDR_CLEAR;
static ldrState_t ldr2_candidate_state = LDR_CLEAR;

// Store when each candidate state was first detected.
static uint32_t ldr1_candidate_since_ms = 0;
static uint32_t ldr2_candidate_since_ms = 0;

// Store the current passage sequence and when the sequence began.
static ldrSequenceState_t sequence_state = LDR_SEQUENCE_IDLE;
static uint32_t ldr_sequence_start_ms = 0;

// Reset the passage sequence without changing the current LDR states.
static void ldr_sequence_reset(void)
{
	sequence_state = LDR_SEQUENCE_IDLE;
	ldr_sequence_start_ms = 0;
}

// Return true while the sequence is still waiting for the second LDR.
// These incomplete sequences can be cancelled after five seconds.
static bool ldr_sequence_can_timeout(void)
{
	return (sequence_state == LDR_SEQUENCE_ENTRY_WAIT_LDR1_CLEAR)||
		   (sequence_state == LDR_SEQUENCE_ENTRY_WAIT_LDR2_BLOCK)||
		   (sequence_state == LDR_SEQUENCE_EXIT_WAIT_LDR2_CLEAR)||
		   (sequence_state == LDR_SEQUENCE_EXIT_WAIT_LDR1_BLOCK);
}

// Convert one raw ADC value into a logical CLEAR or BLOCK state.
// The previous state is included so that the two thresholds can provide hysteresis.
static ldrState_t ldr_adc_to_state(uint16_t adc_value, ldrState_t previous_state, uint16_t block_threshold, uint16_t clear_threshold)
{
	// A blocked LDR must become bright enough to reach the CLEAR threshold.
	if (previous_state == LDR_BLOCK) {
		if (adc_value >= clear_threshold) {
			return LDR_CLEAR;
		}
		return LDR_BLOCK;
	} else {
		// A clear LDR must become dark enough to reach the BLOCK threshold.
		if (adc_value <= block_threshold) {
			return LDR_BLOCK;
		}
		return LDR_CLEAR;
	}
}

// Apply time-based debouncing to one measured LDR state.
// candidate_state and candidate_since_ms are pointers, so this function can update
// the stored candidate information for LDR1 or LDR2.
static ldrState_t ldr_debounce_state(ldrState_t measured_state, ldrState_t stable_state, ldrState_t *candidate_state, uint32_t *candidate_since_ms, uint32_t now)
{
	// The reading returned to the stable state, so any candidate change is cancelled.
	if (measured_state == stable_state) {
		*candidate_state = stable_state;
		*candidate_since_ms = now;
		return stable_state;
	}

	// A different candidate was detected, so begin a new debounce period.
	if (measured_state != *candidate_state) {
		*candidate_state = measured_state;
		*candidate_since_ms = now;
		return stable_state;
	}

	// Accept the candidate only after it remains unchanged for the full debounce time.
	if ((now - *candidate_since_ms) >= LDR_DEBOUNCE_MS) {
		return measured_state;
	}

	return stable_state;
}

// Perform one ADC conversion and write the result through the value pointer.
// Return false if the ADC cannot be started or does not finish within 2 ms.
static bool adc_read_once(ADC_HandleTypeDef *hadc, uint16_t *value)
{
	// Both pointers must be valid before they are used.
	if (hadc == NULL || value == NULL) {
		return false;
	}

	// Start one regular ADC conversion.
	if (HAL_ADC_Start(hadc) != HAL_OK) {
		return false;
	}

	// Wait for the conversion, but do not block for more than 2 ms.
	if (HAL_ADC_PollForConversion(hadc, 2) != HAL_OK) {
		HAL_ADC_Stop(hadc);
		return false;
	}

	// Write the converted value into the variable supplied by the caller.
	*value = (uint16_t)HAL_ADC_GetValue(hadc);
	HAL_ADC_Stop(hadc);

	return true;
}

// Initialize all LDR variables to a known state when the program starts.
void ldr_init(void)
{
	last_ldr_print_ms = 0;

	// Begin with both sensors treated as clear.
	previous_ldr1_state = LDR_CLEAR;
	previous_ldr2_state = LDR_CLEAR;

	latest_ldr1_state = LDR_CLEAR;
	latest_ldr2_state = LDR_CLEAR;

	ldr1_candidate_state = LDR_CLEAR;
	ldr2_candidate_state = LDR_CLEAR;

	ldr1_candidate_since_ms = 0;
	ldr2_candidate_since_ms = 0;

	ldr_sequence_reset();
}
// Read both LDR ADC channels and return the results through two output pointers.
// Return true only when both ADC conversions are successful.
bool ldr_read_raw(uint16_t *ldr1_value, uint16_t *ldr2_value)
{
	// The caller must provide a valid output address for each LDR.
	if (ldr1_value == NULL || ldr2_value == NULL) {
		return false;
	}

	// LDR1 uses ADC2 and LDR2 uses ADC3.
	if (!adc_read_once(&hadc2, ldr1_value)) {
		return false;
	}

	if (!adc_read_once(&hadc3, ldr2_value)) {
		return false;
	}

	return true;
}

// Report whether the physical passage is safe for the door to close.
bool ldr_path_is_clear(void)
{
	// One blocked LDR is enough to keep the path unsafe.
	return (latest_ldr1_state == LDR_CLEAR) && (latest_ldr2_state == LDR_CLEAR);
}

// Poll both sensors, update the passage sequence and return at most one FSM event.
Event_t ldr_poll(void)
{
	// Store the new raw ADC readings for this loop.
	uint16_t ldr1_value;
	uint16_t ldr2_value;

	// EVT_NONE means that no complete state-machine event was detected this loop.
	Event_t event = EVT_NONE;

	// Both readings are required because direction detection uses the two sensors together.
	if (!ldr_read_raw(&ldr1_value, &ldr2_value)) {
		return EVT_NONE;
	}

	// Convert the raw values into measured states, then debounce them to obtain
	// the stable states that are used by edge detection and the sequence state machine.
	uint32_t now = HAL_GetTick();
	ldrState_t ldr1_measured_state = ldr_adc_to_state(ldr1_value, previous_ldr1_state, LDR1_BLOCK_THRESHOLD, LDR1_CLEAR_THRESHOLD);
	ldrState_t ldr2_measured_state = ldr_adc_to_state(ldr2_value, previous_ldr2_state, LDR2_BLOCK_THRESHOLD, LDR2_CLEAR_THRESHOLD);
	ldrState_t ldr1_state = ldr_debounce_state(ldr1_measured_state, previous_ldr1_state, &ldr1_candidate_state, &ldr1_candidate_since_ms, now);
	ldrState_t ldr2_state = ldr_debounce_state(ldr2_measured_state, previous_ldr2_state, &ldr2_candidate_state, &ldr2_candidate_since_ms, now);

	// Each just flag is true for one loop only, when a stable state changes.
	// This prevents one long obstruction from creating the same event repeatedly.
	bool ldr1_just_block = (previous_ldr1_state == LDR_CLEAR) && (ldr1_state == LDR_BLOCK);
	bool ldr1_just_clear = (previous_ldr1_state == LDR_BLOCK) && (ldr1_state == LDR_CLEAR);
	bool ldr2_just_block = (previous_ldr2_state == LDR_CLEAR) && (ldr2_state == LDR_BLOCK);
	bool ldr2_just_clear = (previous_ldr2_state == LDR_BLOCK) && (ldr2_state == LDR_CLEAR);

	// Print each stable edge to make sensor testing easier.
	if (ldr1_just_block) {
			printf("EDGE: LDR1 CLEAR -> BLOCKED\r\n");
	}

	if (ldr1_just_clear) {
		printf("EDGE: LDR1 BLOCKED -> CLEAR\r\n");
	}

	if (ldr2_just_block) {
		printf("EDGE: LDR2 CLEAR -> BLOCKED\r\n");
	}

	if (ldr2_just_clear) {
		printf("EDGE: LDR2 BLOCKED -> CLEAR\r\n");
	}

	// Print the raw values every 200 ms instead of printing during every loop.
	if ((now - last_ldr_print_ms) >= 200U) {
		last_ldr_print_ms = now;

		printf("LDR1=%u LDR2=%u\r\n", (unsigned int)ldr1_value, (unsigned int)ldr2_value);
	}

	// Use the order of the stable edges to update the passage sequence.
	switch(sequence_state)
	{
		case LDR_SEQUENCE_IDLE:

			// Both inside and outside LDR blocked
			if (ldr1_just_block && ldr2_just_block) {
				sequence_state = LDR_SEQUENCE_SIMULTANEOUS_WAIT_CLEAR;
				event = EVT_BOTH_LDRS_BLOCKED;

				printf("Simultaneous request: both LDRs blocked\r\n");
			}

			// Outside building's LDR blocked, may indicate for entry.
			else if (ldr1_just_block) {
				sequence_state = LDR_SEQUENCE_ENTRY_WAIT_LDR1_CLEAR;
				ldr_sequence_start_ms = now;
				event = EVT_ENTRY_REQUEST;

				printf("Entry sequence started: waiting for LDR1 to clear\r\n");
			}

			// Inside building's LDR blocked, may indicate for exit.
			else if (ldr2_just_block) {
				sequence_state = LDR_SEQUENCE_EXIT_WAIT_LDR2_CLEAR;
				ldr_sequence_start_ms = now;
				event = EVT_EXIT_REQUEST;

				printf("Exit sequence started: waiting for LDR2 to clear\r\n");
			}

			break;

		case LDR_SEQUENCE_ENTRY_WAIT_LDR1_CLEAR:

			// If LDR2 becomes blocked while LDR1 still blocked
			if (ldr2_just_block && (ldr1_state == LDR_BLOCK)) {
				sequence_state = LDR_SEQUENCE_SIMULTANEOUS_WAIT_CLEAR;
				event = EVT_BOTH_LDRS_BLOCKED;

				printf("Simultaneous request detected during entry\r\n");
			}

			// LDR1 cleared and LDR2 blocked in a same polling cycle.
			else if (ldr2_just_block) {
				sequence_state = LDR_SEQUENCE_ENTRY_WAIT_LDR2_CLEAR;
				event = EVT_ENTRY_CONFIRMED;

				printf("Direction confirmed: ENTRY\r\n");
			}

			// LDR1 cleared and waiting for LDR2 blocked.
			else if (ldr1_just_clear) {
				sequence_state = LDR_SEQUENCE_ENTRY_WAIT_LDR2_BLOCK;

				printf("Entry sequence: waiting for LDR2 to block\r\n");
			}

			break;

		case LDR_SEQUENCE_ENTRY_WAIT_LDR2_BLOCK:

			// Atfer LDR1 blocked and cleared once, this outside LDR blocked again, what means there is a tailgating.
			if (ldr2_just_block && (ldr1_state == LDR_BLOCK)) {
				sequence_state = LDR_SEQUENCE_TAILGATE_WAIT_CLEAR;
				event = EVT_TAILGATE_DETECTED;

				printf("Tailgating detected across both LDRs\r\n");
			}

			// Inside LDR blocked, the entry direction can be confirmed.
			else if (ldr2_just_block) {
				sequence_state = LDR_SEQUENCE_ENTRY_WAIT_LDR2_CLEAR;
				event = EVT_ENTRY_CONFIRMED;

				printf("Entry confirmed: waiting for LDR2 to clear\r\n");
			}

			break;

		case LDR_SEQUENCE_ENTRY_WAIT_LDR2_CLEAR:

			// Atfer LDR1 blocked and cleared once, this outside LDR blocked again, what means there is a tailgating.
			if (ldr1_just_block) {
				sequence_state = LDR_SEQUENCE_TAILGATE_WAIT_CLEAR;
				event = EVT_TAILGATE_DETECTED;

				printf("Tailgating detected: LDR1 blocked again\r\n");
			}

			// Entry is complete only after the person has cleared both sensors.
			else if ((ldr1_state == LDR_CLEAR) && (ldr2_state == LDR_CLEAR)) {
				ldr_sequence_reset();
				event = EVT_PASSAGE_DONE;

				printf("ENTRY passage complete\r\n");
			}

			break;

		case LDR_SEQUENCE_EXIT_WAIT_LDR2_CLEAR:

			//If LDR1 becomes blocked while LDR2 still blocked
			if (ldr1_just_block && (ldr2_state == LDR_BLOCK)) {
				sequence_state = LDR_SEQUENCE_SIMULTANEOUS_WAIT_CLEAR;
				event = EVT_BOTH_LDRS_BLOCKED;

				printf("Simultaneous request detected during exit\r\n");
			}

			// LDR2 cleared and LDR1 blocked in the same polling cycle.
			else if (ldr1_just_block) {
				sequence_state = LDR_SEQUENCE_EXIT_WAIT_LDR1_CLEAR;

				printf("Direction confirmed: EXIT\r\n");
			}

			// LDR2 cleared and waiting for LDR1 blocked.
			else if (ldr2_just_clear) {
				sequence_state = LDR_SEQUENCE_EXIT_WAIT_LDR1_BLOCK;

				printf("Exit sequence: waiting for LDR1 to block\r\n");
			}

			break;

		case LDR_SEQUENCE_EXIT_WAIT_LDR1_BLOCK:

			// After LDR2 cleared then blocking LDR1
			if (ldr1_just_block) {
				sequence_state = LDR_SEQUENCE_EXIT_WAIT_LDR1_CLEAR;

				printf("Exit confirmed: waiting for LDR1 to clear\r\n");
			}

			break;

		case LDR_SEQUENCE_EXIT_WAIT_LDR1_CLEAR:

			// Both LDRs are cleared. User have already passage the door.
			if ((ldr1_state == LDR_CLEAR) && (ldr2_state == LDR_CLEAR)) {
				ldr_sequence_reset();
				event = EVT_PASSAGE_DONE;

				printf("Exit passage complete\r\n");
			}

			break;

		case LDR_SEQUENCE_SIMULTANEOUS_WAIT_CLEAR:

			if ((ldr1_state == LDR_CLEAR) && ldr2_state == LDR_CLEAR){
				ldr_sequence_reset();
				event = EVT_PASSAGE_CANCELLED;

				printf("Passage clear after simultaneous request\r\n");
			}

			break;

		case LDR_SEQUENCE_TAILGATE_WAIT_CLEAR:

			// The main FSM controls the buzzer and warning light.
			// The LDR sequence waits until the physical passage is clear
			if ((ldr1_state == LDR_CLEAR) && (ldr2_state == LDR_CLEAR)) {
				ldr_sequence_reset();

				printf("Passage clear after tailgating alert\r\n");
			}

			break;

		default:
			ldr_sequence_reset();

			break;
	}

	// Cancel only an incomplete direction sequence.
	// Once the second LDR has been reached, the program waits for the person
	// to clear the passage instead of closing the door because of a timeout.
	if (ldr_sequence_can_timeout() &&
	    ((now - ldr_sequence_start_ms) >= LDR_SEQUENCE_TIMEOUT_MS))
	{
	    printf("LDR sequence timeout: passage cancelled\r\n");

	    ldr_sequence_reset();
	    event = EVT_PASSAGE_CANCELLED;
	}

	// Save the latest stable states for the FSM closing safety check.
	latest_ldr1_state = ldr1_state;
	latest_ldr2_state = ldr2_state;

	// Save the same states as previous so the next loop can detect new edges.
	previous_ldr1_state = ldr1_state;
	previous_ldr2_state = ldr2_state;

	return event;
}

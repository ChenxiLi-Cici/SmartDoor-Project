#include "ldr.h"
#include "main.h"
#include <stdio.h>

#define LDR1_BLOCK_THRESHOLD 900U
#define LDR1_CLEAR_THRESHOLD 1200U
#define LDR2_BLOCK_THRESHOLD 900U
#define LDR2_CLEAR_THRESHOLD 1200U

#define LDR_SEQUENCE_TIMEOUT_MS 5000U
#define LDR_DEBOUNCE_MS 20U

typedef enum
{
	LDR_SEQUENCE_IDLE,
	LDR_SEQUENCE_LDR1_FIRST,
	LDR_SEQUENCE_LDR2_FIRST,
	LDR_SEQUENCE_WAIT_CLEAR,
	LDR_SEQUENCE_EXIT,
	LDR_SEQUENCE_ENTRY
} ldrSequenceState_t;

extern ADC_HandleTypeDef hadc2;  /* LDR1: PC4, ADC2 Channel 5 */
extern ADC_HandleTypeDef hadc3;  /* LDR2: PB1, ADC3 Channel 1 */

static bool ldr_is_armed = false;
static uint32_t last_ldr_print_ms = 0;

static ldrState_t previous_ldr1_state = LDR_CLEAR;
static ldrState_t previous_ldr2_state = LDR_CLEAR;

static ldrState_t latest_ldr1_state = LDR_CLEAR;
static ldrState_t latest_ldr2_state = LDR_CLEAR;

static ldrState_t ldr1_candidate_state = LDR_CLEAR;
static ldrState_t ldr2_candidate_state = LDR_CLEAR;

static uint32_t ldr1_candidate_since_ms = 0;
static uint32_t ldr2_candidate_since_ms = 0;

static ldrSequenceState_t sequence_state = LDR_SEQUENCE_IDLE;
static uint32_t ldr_sequence_start_ms = 0;

static void ldr_sequence_reset(void)
{
	sequence_state = LDR_SEQUENCE_IDLE;
	ldr_sequence_start_ms = 0;
}

static ldrState_t ldr_adc_to_state(uint16_t adc_value, ldrState_t previous_state, uint16_t block_threshold, uint16_t clear_threshold)
{
	if (previous_state == LDR_BLOCK) {
		if (adc_value >= clear_threshold) {
			return LDR_CLEAR;
		}
		return LDR_BLOCK;
	}

	else if(previous_state == LDR_CLEAR) {
		if (adc_value <= block_threshold) {
			return LDR_BLOCK;
		}
		return LDR_CLEAR;
	}
}

static ldrState_t ldr_debounce_state(ldrState_t measured_state, ldrState_t stable_state, ldrState_t *candidate_state, uint32_t *candidate_since_ms, uint32_t now)
{
	if (measured_state == stable_state) {
		*candidate_state = stable_state;
		*candidate_since_ms = now;
		return stable_state;
	}

	if (measured_state != *candidate_state) {
		*candidate_state = measured_state;
		*candidate_since_ms = now;
		return stable_state;
	}

	if ((now - *candidate_since_ms) >= LDR_DEBOUNCE_MS) {
		return measured_state;
	}

	return stable_state;
}

static bool adc_read_once(ADC_HandleTypeDef *hadc, uint16_t *value)
{
	if (hadc == NULL || value == NULL) {
		return false;
	}

	if (HAL_ADC_Start(hadc) != HAL_OK) {
		return false;
	}

	if (HAL_ADC_PollForConversion(hadc, 2) != HAL_OK) {
		HAL_ADC_Stop(hadc);
		return false;
	}

	*value = (uint16_t)HAL_ADC_GetValue(hadc);
	HAL_ADC_Stop(hadc);

	return true;
}

void ldr_init(void)
{
	ldr_is_armed = false;
	last_ldr_print_ms = 0;

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

void ldr_arm(bool armed)
{
	ldr_is_armed = armed;

	previous_ldr1_state = LDR_CLEAR;
	previous_ldr2_state = LDR_CLEAR;

	latest_ldr1_state = LDR_CLEAR;
	latest_ldr2_state = LDR_CLEAR;

	ldr1_candidate_state = LDR_CLEAR;
	ldr2_candidate_state = LDR_CLEAR;

	ldr1_candidate_since_ms = 0;
	ldr2_candidate_since_ms = 0;

	ldr_sequence_reset();

	if (armed) {
		/*
		 * Later we will reset filtering, edge-detection
		 * and passage-tracking state here.
		 */
	}
}

bool ldr_read_raw(uint16_t *ldr1_value, uint16_t *ldr2_value)
{
	if (ldr1_value == NULL || ldr2_value == NULL) {
		return false;
	}

	if (!adc_read_once(&hadc2, ldr1_value)) {
		return false;
	}

	if (!adc_read_once(&hadc3, ldr2_value)) {
		return false;
	}

	return true;
}

bool ldr_path_is_clear(void)
{
	return (latest_ldr1_state == LDR_CLEAR) && (latest_ldr2_state == LDR_CLEAR);
}

Event_t ldr_poll(void)
{
	uint16_t ldr1_value;
	uint16_t ldr2_value;
	Event_t event = EVT_NONE;

	if (!ldr_is_armed) {
		return EVT_NONE;
	}

	if (!ldr_read_raw(&ldr1_value, &ldr2_value)) {
		return EVT_NONE;
	}

	uint32_t now = HAL_GetTick();
	ldrState_t ldr1_measured_state = ldr_adc_to_state(ldr1_value, previous_ldr1_state, LDR1_BLOCK_THRESHOLD, LDR1_CLEAR_THRESHOLD);
	ldrState_t ldr2_measured_state = ldr_adc_to_state(ldr2_value, previous_ldr2_state, LDR2_BLOCK_THRESHOLD, LDR2_CLEAR_THRESHOLD);
	ldrState_t ldr1_state = ldr_debounce_state(ldr1_measured_state, previous_ldr1_state, &ldr1_candidate_state, &ldr1_candidate_since_ms, now);
	ldrState_t ldr2_state = ldr_debounce_state(ldr2_measured_state, previous_ldr2_state, &ldr2_candidate_state, &ldr2_candidate_since_ms, now);

	bool ldr1_just_block = (previous_ldr1_state == LDR_CLEAR) && (ldr1_state == LDR_BLOCK);
	bool ldr1_just_clear = (previous_ldr1_state == LDR_BLOCK) && (ldr1_state == LDR_CLEAR);
	bool ldr2_just_block = (previous_ldr2_state == LDR_CLEAR) && (ldr2_state == LDR_BLOCK);
	bool ldr2_just_clear = (previous_ldr2_state == LDR_BLOCK) && (ldr2_state == LDR_CLEAR);

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

	if ((now - last_ldr_print_ms) >= 200U) {
		last_ldr_print_ms = now;

		printf("LDR1=%u LDR2=%u\r\n", (unsigned int)ldr1_value, (unsigned int)ldr2_value);
	}

	switch(sequence_state)
	{
		case LDR_SEQUENCE_IDLE:
			if (ldr1_just_block && ldr2_just_block) {
				sequence_state = LDR_SEQUENCE_WAIT_CLEAR;
				event = EVT_BOTH_LDRS_BLOCKED;
				printf("Direction ambiguous: both LDRs blocked together\r\n");
			}
			else if (ldr1_just_block) {
				sequence_state = LDR_SEQUENCE_LDR1_FIRST;
				ldr_sequence_start_ms = now;
				event = EVT_ENTRY_REQUEST;
				printf("Sequence started: LDR1 first\r\n");
			}
			else if (ldr2_just_block) {
				sequence_state = LDR_SEQUENCE_LDR2_FIRST;
				ldr_sequence_start_ms = now;
				event = EVT_EXIT_REQUEST;
				printf("Sequence started: LDR2 first\r\n");
			}
			break;

		case LDR_SEQUENCE_LDR1_FIRST:
			if (ldr2_just_block) {
				sequence_state = LDR_SEQUENCE_ENTRY;
				event = EVT_ENTRY_CONFIRMED;
				printf("Direction confirmed: ENTRY\r\n");
			}
			else if (ldr1_just_clear && (ldr2_state == LDR_CLEAR)) {
				ldr_sequence_reset();
				event = EVT_PASSAGE_CANCELLED;
				printf("ENTRY request cancelled\r\n");
			}
			break;

		case LDR_SEQUENCE_LDR2_FIRST:
			if (ldr1_just_block) {
				sequence_state = LDR_SEQUENCE_EXIT;
				event = EVT_EXIT_CONFIRMED;
				printf("Direction confirmed: EXIT\r\n");
			}
			else if (ldr2_just_clear && (ldr1_state == LDR_CLEAR)) {
				ldr_sequence_reset();
				event = EVT_PASSAGE_CANCELLED;
				printf("EXIT request cancelled\r\n");
			}
			break;

		case LDR_SEQUENCE_ENTRY:
			if ((ldr1_state == LDR_CLEAR) && (ldr2_state == LDR_CLEAR)) {
				ldr_sequence_reset();
				event = EVT_PASSAGE_DONE;
				printf("ENTRY passage complete\r\n");
			}
			break;

		case LDR_SEQUENCE_EXIT:
			if ((ldr1_state == LDR_CLEAR) && (ldr2_state == LDR_CLEAR)) {
				ldr_sequence_reset();
				event = EVT_PASSAGE_DONE;
				printf("EXIT passage complete\r\n");
			}
			break;

		case LDR_SEQUENCE_WAIT_CLEAR:
			if ((ldr1_state == LDR_CLEAR) && (ldr2_state == LDR_CLEAR)) {
				ldr_sequence_reset();
				event = EVT_PASSAGE_CANCELLED;
				printf("Path clear after ambiguous or timed-out sequence\r\n");
			}
			break;

		default:
			ldr_sequence_reset();
			break;
	}

	if (((sequence_state == LDR_SEQUENCE_LDR1_FIRST) || (sequence_state == LDR_SEQUENCE_LDR2_FIRST)) && (now - ldr_sequence_start_ms) >= LDR_SEQUENCE_TIMEOUT_MS)
	{
		printf("LDR sequence timeout - reset\r\n");
		ldr_sequence_reset();
		event = EVT_PASSAGE_CANCELLED;
	}

	latest_ldr1_state = ldr1_state;
	latest_ldr2_state = ldr2_state;

	previous_ldr1_state = ldr1_state;
	previous_ldr2_state = ldr2_state;

	return event;
}

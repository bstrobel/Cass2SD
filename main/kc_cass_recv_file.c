/*
 * kc_cass_recv_file.c
 *
 * Created: 06.06.2017 19:11:18
 *  Author: Bernd
 */ 
#include <avr/io.h>
#include <avr/interrupt.h>
#include <avr/sfr_defs.h>
#define __PROG_TYPES_COMPAT__
#include <avr/pgmspace.h>
#include <string.h>
#include <util/delay.h>
#include <stdbool.h>
#include "kc_cass_common.h"
#include "display_util.h"
#include "kc_cass_recv_file.h"

#define RECV_TIMER_TIMEOUT_CNT 16000
#define RECV_TIMER_ZERO_THRESHOLD 3000
#define RECV_TIMER_ONE_THRESHOLD 6000
#define MIN_ONE_BITS_IN_VORTON 9

volatile bool is_first_block = false;
volatile bool is_in_recv_state = false;
volatile uint8_t buf_idx = 0;
char file_name[13]; // contains 8byte name + 1 dot + 3byte ext + 0x00 byte

volatile bool is_begin_measure = true;
volatile bool is_byte_complete = false;
volatile bool is_start_new_block = false;
volatile uint16_t cntr_val = RECV_TIMER_TIMEOUT_CNT;
volatile bool timeout_occured = false;
volatile uint8_t recv_byte = 0;
volatile uint8_t next_bit = 0;

static void config_and_start_timer_recv_file() {
	OCR1A = RECV_TIMER_TIMEOUT_CNT; // set TOP of counter to be reached after 2msec
	TCNT1 = 0; // reset counter 
	TIFR1 = 0; // clear interrupt flags
	TIMSK1 = _BV(OCIE1A); // allow interrupts for OCR1A match
	TCCR1A = 0;
	TCCR1B = _BV(WGM12) || _BV(CS10); // Enable timer in CTC-Mode, pre-scaler = 1
}

static void stop_timer_recv_file() {
	TCCR1B = 0;
	TCCR1A = 0;
	TIMSK1 = 0;
	TIFR1 = 0;
}

// should only be fired when timer is not reset before,
// meaning this is a timeout (doesn't need to be an error,
// can be at end of file
ISR(TIMER1_COMPA_vect) {
	stop_timer_recv_file();
	timeout_occured = true;
}

// is fired at each change in logical level of pin
ISR(INT0_vect) {
	if (is_begin_measure) {
		// initialize and start timer
		config_and_start_timer_recv_file();
		is_begin_measure = false;
	} else {
		// stop timer and shift in bit
		stop_timer_recv_file();
		cntr_val = TCNT1;
		if (cntr_val > RECV_TIMER_ONE_THRESHOLD) {
			// we have a SPACE, byte is over
			is_byte_complete = true;
			next_bit = 0;
		}
		else if(!is_byte_complete) {
			recv_byte >>= 1;
			if (cntr_val > RECV_TIMER_ZERO_THRESHOLD && cntr_val < RECV_TIMER_ONE_THRESHOLD) {
				recv_byte += 0b10000000;
			}
			next_bit++;
		}
		if (next_bit > MIN_ONE_BITS_IN_VORTON && recv_byte == 0b11111111) {
			// KC tries to send us some data -> get ready
			is_start_new_block = true;
		}
		is_begin_measure = true;
	}
}

void kc_cass_recv_file_init() {
	CASS_IN_DDR &= ~_BV(CASS_IN_PIN); // Configure input pin as INPUT
	CASS_IN_PORT &= ~_BV(CASS_IN_PIN); // Disable pullup
	EIFR &= _BV(INT0); // clear int flag reg
	EIMSK |= _BV(INT0); // enable int INT0
	EICRA = _BV(ISC00); // any change in logical level on pin will generate int
}

void kc_cass_recv_file_disable() {
	EIMSK &= ~_BV(INT0); // make sure INT0 pin interrupts are disabled
	CASS_IN_DDR &= ~_BV(CASS_IN_PIN); // Configure input pin as INPUT
	CASS_IN_PORT |= _BV(CASS_IN_PIN); // Enable pullup
}

static void copy_filename() {
	KC_FCB_MC* fcb = (KC_FCB_MC*) (buf + 1);
	uint8_t j = 0;
	bool early_dot = false;
	for (uint8_t i = 0; i < 8; i++) {
		if (fcb->dateiname[i] < 0x21 || fcb->dateiname[i] > 0x7f) {
			file_name[j] = '.';
			j++;
			early_dot = true;
			break;
		}
		file_name[j] = fcb->dateiname[i];
		j++;
	}
	if (!early_dot) {
		file_name[j] = '.';
		j++;
	}
	for (uint8_t i = 0; i < 3; i++) {
		if (fcb->dateityp[i] < 0x21 || fcb->dateityp[i] > 0x7f) {
			j++;
			break;
		}
		file_name[j] = fcb->dateityp[i];
		j++;
	}
	file_name[j] = 0;
}

static void reset_state() {
	timeout_occured = false;
	is_in_recv_state = false;
	is_start_new_block = false;
	is_first_block = false;
}

/************************************************************************/
/* To be called from the main while loop                                */
/************************************************************************/
void kc_cass_handle_recv_file() {
	if (is_start_new_block) {
		if (!is_in_recv_state) {
			is_in_recv_state = true;
			memset(buf,0x0,DATA_BUF_SIZE);
			buf_idx = 0;
			is_first_block = true;
			// TODO: block input and take over DISPLAY
		}
		else {
			// TODO: check checksum
			UINT bytes_written = 0;
			if (is_first_block) {
				// we assume there is always a real FCB in the beginning
				copy_filename();
				f_close(&fhdl);
				if (disp_fr_err(f_open(&fhdl, file_name, FA_WRITE | FA_CREATE_ALWAYS))) {
					f_close(&fhdl);
					// TODO: display better error
					reset_state();
					return;
				}

				// write block to file
				if (disp_fr_err(f_write(&fhdl, buf, DATA_BUF_SIZE, &bytes_written))) {
					f_close(&fhdl);
					// TODO: display better error
					reset_state();
					return;
				}
				is_first_block = false;
			}
			else {
				// append block to open file
				if (disp_fr_err(f_write(&fhdl, buf, DATA_BUF_SIZE, &bytes_written))) {
					f_close(&fhdl);
					// TODO: display better error
					reset_state();
					return;
				}
			}
			memset(buf,0x0,DATA_BUF_SIZE);
			buf_idx = 0;
		}
		is_start_new_block = false;
		return;
	}
	if (is_in_recv_state && is_byte_complete) {
		is_byte_complete = false;
		if (buf_idx < DATA_BUF_SIZE) {
			buf[buf_idx] = recv_byte;
			buf_idx++;
		}
		return;
	}
	if (timeout_occured && is_in_recv_state) {
		f_close(&fhdl);
		// TODO: display result
		reset_state();
		return;
	}
}
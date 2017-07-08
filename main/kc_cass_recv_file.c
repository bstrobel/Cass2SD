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
#include <stddef.h>
#include <stdbool.h>
#include "kc_cass_common.h"
#include "display_util.h"
#include "kc_cass_recv_file.h"

#define RECV_TIMER_TIMEOUT_CNT 16000
#define RECV_TIMER_ZERO_THRESHOLD 3000
#define RECV_TIMER_ONE_THRESHOLD 6000
#define MIN_ONE_BITS_IN_VORTON 9
#define DISK_BLOCK_SIZE (DATA_BUF_SIZE - 1)

volatile bool is_first_block = false;
volatile bool is_in_recv_state = false;
volatile uint8_t buf_idx = 0;
char file_name[13]; // contains 8byte name + 1 dot + 3byte ext + 0x00 byte

volatile bool is_begin_measure = true; // if true ISR(INT0_vect) starts a new time measurement
volatile bool is_byte_complete = false; // used in the ISR(INT0_vect) and the handler to signal if a byte has been received completely
volatile bool is_start_new_block = false; // used in the ISR(INT0_vect) to signal to the handler that a BLOCK VORTON has been detected and a new block should start
volatile bool timeout_occured = false;
volatile uint8_t recv_byte = 0;
volatile uint8_t next_bit = 0;
volatile uint16_t vorton_cntr = 0;

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

static void reset_state() {
	stop_timer_recv_file();

	is_first_block = false;
	is_in_recv_state = false;
	buf_idx = 0;
	file_name[0] = 0;

	is_begin_measure = true;
	is_byte_complete = false;
	is_start_new_block = false;
	timeout_occured = false;
	recv_byte = 0;
	next_bit = 0;
	vorton_cntr = 0;
}

static void get_filename_from_fcb() {
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

// should only be fired when timer is not reset before,
// meaning this is a timeout (doesn't need to be an error,
// can be at end of file)
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
		uint16_t cntr_val = TCNT1;
		if (cntr_val > RECV_TIMER_ONE_THRESHOLD) { // we have a SPACE
			// we have a SPACE, byte is over
			is_byte_complete = true;
			next_bit = 0;
			if (vorton_cntr > MIN_ONE_BITS_IN_VORTON) {
				// KC tries to send us some data -> get ready
				is_start_new_block = true;
				#ifdef DEBUG_RECV_TIMER
				MONITOR_RECV_PORT |= _BV(MONITOR_RECV_BIT);
				#endif
			}
		}
		else if(!is_byte_complete) { // we have a ONE or a ZERO and the byte is not complete yet
			recv_byte >>= 1;
			if (cntr_val > RECV_TIMER_ZERO_THRESHOLD && cntr_val <= RECV_TIMER_ONE_THRESHOLD) {
				recv_byte += 0b10000000;
				vorton_cntr++;
			}
			else {
				vorton_cntr = 0;
			}
			next_bit++;
		}
		is_begin_measure = true;
	}
	#ifdef DEBUG_RECV_TIMER
	MONITOR_RECV_PORT &= ~_BV(MONITOR_RECV_BIT);
	#endif
}

void kc_cass_recv_file_init() {
	#ifdef DEBUG_RECV_TIMER
	MONITOR_RECV_DDR |= _BV(MONITOR_RECV_BIT);
	MONITOR_RECV_PORT &= ~_BV(MONITOR_RECV_BIT);
	#endif
	
	CASS_IN_DDR &= ~_BV(CASS_IN_PIN); // Configure input pin as INPUT
	CASS_IN_PORT &= ~_BV(CASS_IN_PIN); // Disable pullup
	EIFR &= _BV(INT0); // clear int flag reg
	EIMSK |= _BV(INT0); // enable int INT0
	EICRA = _BV(ISC00); // any change in logical level on pin will generate int
	reset_state();
}

void kc_cass_recv_file_disable() {
	EIMSK &= ~_BV(INT0); // make sure INT0 pin interrupts are disabled
	CASS_IN_DDR &= ~_BV(CASS_IN_PIN); // Configure input pin as INPUT
	CASS_IN_PORT |= _BV(CASS_IN_PIN); // Enable pullup
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
			display_recvinfo(NULL,0);
		}
		else {
			// is_start_new_block == true and is_in_recv_state -> a block is complete, write it
			if (buf[129] != calculate_checksum()) {
				reset_state();
				f_close(&fhdl);
				disp_err("ERROR","WRONG CHECKSUM");
				return;
			}
			UINT bytes_written = 0;
			if (is_first_block) {
				// we assume there is always a real FCB in the beginning
				// TODO: check what about BASIC!
				get_filename_from_fcb();
				f_close(&fhdl);
				fr = f_open(&fhdl, file_name, FA_WRITE | FA_CREATE_ALWAYS);
				if (fr != FR_OK) {
					reset_state();
					f_close(&fhdl);
					disp_fr_err(fr);
					disp_err("ERROR","OPEN FOR WRITE");
					return;
				}
				display_recvinfo(file_name,buf[0]);

				// write block to file
				fr = f_write(&fhdl, buf, DISK_BLOCK_SIZE - 1, &bytes_written);
				if (fr != FR_OK || bytes_written != DISK_BLOCK_SIZE) {
					reset_state();
					f_close(&fhdl);
					if (fr != FR_OK) {
						disp_fr_err(fr);
					}
					disp_err("ERROR","WRITE FILE");
					return;
				}
				is_first_block = false;
			}
			else {
				// append block to open file
				display_upd_recvinfo(buf[0]);
				fr = f_write(&fhdl, buf, DISK_BLOCK_SIZE, &bytes_written);
				if (fr != FR_OK || bytes_written != DISK_BLOCK_SIZE) {
					reset_state();
					f_close(&fhdl);
					if (fr != FR_OK) {
						disp_fr_err(fr);
					}	
					disp_err("ERROR","APPEND BLOCK");
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
		else {
			reset_state();
			f_close(&fhdl);
			disp_err("ERROR","BLKSIZE EXCEEDED");
			return;
		}
		return;
	}
	if (timeout_occured && is_in_recv_state) {
		reset_state();
		f_close(&fhdl);
		disp_err("INFO","FILE SAVED");
		return;
	}
}
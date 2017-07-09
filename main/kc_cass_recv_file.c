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
#include "../ff_avr/xitoa.h"
#include "../lcd/lcd.h"

#define RECV_TIMER_TIMEOUT_CNT 16000
#define RECV_TIMER_ZERO_THRESHOLD 3000
#define RECV_TIMER_ONE_THRESHOLD 6000
#define MIN_ONE_BITS_IN_VORTON 20
#define DISK_BLOCK_SIZE (DATA_BUF_SIZE - 1)
#define IS_SPACE(ctr) (ctr > RECV_TIMER_ONE_THRESHOLD)
#define IS_ONE(ctr) (ctr > RECV_TIMER_ZERO_THRESHOLD && ctr <= RECV_TIMER_ONE_THRESHOLD)
#define IS_ZERO(ctr) (ctr <= RECV_TIMER_ZERO_THRESHOLD)
#define MAX_BUF_IDX (DATA_BUF_SIZE - 1)

typedef enum {
	RECV_VORTON_DETECTED, 
	RECV_BYTE_READY, 
	RECV_BIT_TIMEOUT, 
	RECV_HANDLER_ACK, 
	RECV_OVERRUN_OCCURED
} recv_state_enum;
volatile recv_state_enum recv_state = RECV_HANDLER_ACK;

volatile bool is_in_recv_state = false;
volatile uint8_t buf_idx = 0;
char file_name_on_disk[LEN_DATEINAME + 1 + LEN_DATEITYP + 1]; // contains 8byte name + 1 dot + 3byte ext + 0x00byte
char file_type[LEN_DATEITYP + 1]; // 3byte + 0x00byte
const char msg_info_saved_str[] PROGMEM = "INFO: FILE SAVED";

volatile bool is_begin_measure = true; // if true ISR(INT0_vect) starts a new time measurement
volatile uint8_t recv_byte = 0;
volatile uint8_t block_cntr = 0;
volatile uint16_t vorton_cntr = 0;

static void config_and_start_timer_recv_file() {
	OCR1A = RECV_TIMER_TIMEOUT_CNT; // set TOP of counter to be reached after 2msec
	TCNT1 = 0; // reset counter 
	TIFR1 = 0; // clear interrupt flags
	TIMSK1 = _BV(OCIE1A); // allow interrupts for OCR1A match
	TCCR1A = 0;
	TCCR1B = _BV(WGM12) || _BV(CS10); // Enable timer in CTC-Mode, pre-scaler = 1, 8MHz timer clock
}

static void stop_timer_recv_file() {
	TCCR1B = 0;
	TCCR1A = 0;
	TIMSK1 = 0;
	TIFR1 = 0;
}

static void reset_state_after_error_or_end() {
	is_in_recv_state = false;
	block_cntr = 0;
	buf_idx = 0;
	file_name_on_disk[0] = 0;
	#ifdef DEBUG_RECV_TIMER
	MONITOR_RECV_PORT &= ~_BV(MONITOR_RECV_BIT);
	#endif
}

// should only be fired when timer is not reset before,
// meaning this is a timeout (doesn't need to be an error,
// can be at end of file)
ISR(TIMER1_COMPA_vect) {
	stop_timer_recv_file();
	if (recv_state == RECV_HANDLER_ACK) {
		recv_state = RECV_BIT_TIMEOUT;
	}
	else {
		recv_state = RECV_OVERRUN_OCCURED;
	}
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
		
		if (recv_state != RECV_HANDLER_ACK) {
			recv_state = RECV_OVERRUN_OCCURED;
		}
		else {
			if (IS_SPACE(cntr_val)) {
				if (vorton_cntr > MIN_ONE_BITS_IN_VORTON) {
					recv_state = RECV_VORTON_DETECTED;
					#ifdef DEBUG_RECV_TIMER
					MONITOR_RECV_PORT |= _BV(MONITOR_RECV_BIT);
					#endif
				}
				else {
					recv_state = RECV_BYTE_READY;
				}
				vorton_cntr = 0;
			}
			else {
				recv_byte >>= 1;
				if (IS_ONE(cntr_val)) {
					vorton_cntr++;
					recv_byte += 0b10000000;
					} else if (IS_ZERO(cntr_val)) {
					vorton_cntr=0;
				}
			}
		}
		is_begin_measure = true;
	}
}

static bool get_filename_from_fcb() {
	char* dateiname;
	char* dateityp;
	if (check_is_basic_fcb()) {
		KC_FCB_BASIC* fcb = (KC_FCB_BASIC*) (buf + 1);
		dateiname = fcb->dateiname;
		dateityp = fcb->dateityp;
		for (uint8_t i = 0; i < LEN_DATEITYP; i++) {
			if (dateityp[i] < 0xd3 || dateityp[i] > 0xd9) {
				file_type[i] = 0;
				break;
			}
			file_type[i] = dateityp[i] - 0x80;
		}
	}
	else {
		KC_FCB_MC* fcb = (KC_FCB_MC*) (buf + 1);
		dateiname = fcb->dateiname;
		dateityp = fcb->dateityp;
		for (uint8_t i = 0; i < LEN_DATEITYP; i++) {
			if (dateityp[i] < 0x21 || dateityp[i] > 0x7f) {
				file_type[i] = 0;
				break;
			}
			file_type[i] = dateityp[i];
		}
	}
	file_type[LEN_DATEITYP]=0;
	uint8_t i = 0;
	for (; i < LEN_DATEINAME; i++) {
		if (dateiname[i] < 0x21 || dateiname[i] > 0x7f) {
			break;
		}
		file_name_on_disk[i] = dateiname[i];
	}
	if (i == 0) {
		return false; // zero length filename -> something is wrong
	}
	else {
		file_name_on_disk[i] = '.';
		strlcpy_P(file_name_on_disk+i+1,PSTR(FILE_EXT),4);
		return true;
	}
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
	reset_state_after_error_or_end();
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
	switch (recv_state) {
		case RECV_OVERRUN_OCCURED: {
			reset_state_after_error_or_end();
			f_close(&fhdl);
			if (is_in_recv_state) {
				disp_msg_p(msg_error_str,PSTR("OVERRUN!")); // we are only interested in overruns during receive
			}
			break;
		}
		case RECV_VORTON_DETECTED: {
			memset(buf,0x0,DATA_BUF_SIZE);
			buf_idx = 0;
			if (!is_in_recv_state) {
				is_in_recv_state = true;
				block_cntr = 0;
				display_recvinfo(NULL,0);
			}
			break;
		}
		case RECV_BYTE_READY: {
			#ifdef DEBUG_RECV_TIMER
			MONITOR_RECV_PORT &= ~_BV(MONITOR_RECV_BIT);
			#endif
			if (is_in_recv_state) {
				buf_idx++;
				if (buf_idx < MAX_BUF_IDX) {
					buf[buf_idx] = recv_byte;
					break;
				}
				if (buf_idx == MAX_BUF_IDX) { // block completely received
					if (recv_byte != calculate_checksum()) {
						reset_state_after_error_or_end();
						f_close(&fhdl);
						lcd_clrscr();
						xprintf(PSTR("WRONG CHECKSUM"));
						lcd_gotoxy(0,1);
						xprintf(PSTR("rcv=%d clc=%d"), recv_byte, calculate_checksum());
						_delay_ms(ERROR_DISP_MILLIS);
						break;
					}

					if (block_cntr == 0) { // first block
						f_close(&fhdl); // just to be sure
						if (!get_filename_from_fcb()) {
							reset_state_after_error_or_end();
							disp_msg_p(msg_error_str,PSTR("FILENAME=\"\""));
							break;
						}
						else {
							fr = f_open(&fhdl, file_name_on_disk, FA_WRITE | FA_CREATE_ALWAYS);
							if (fr != FR_OK) {
								reset_state_after_error_or_end();
								f_close(&fhdl);
								disp_fr_err(fr);
								disp_msg_p(msg_error_str,PSTR("OPEN FOR WRITE"));
								break;
							}
							else {
								display_recvinfo(file_name_on_disk,buf[0]);
							}
						}
					}

					UINT bytes_written = 0;
					fr = f_write(&fhdl, buf, DISK_BLOCK_SIZE, &bytes_written);
					if (fr != FR_OK || bytes_written != DISK_BLOCK_SIZE) {
						reset_state_after_error_or_end();
						f_close(&fhdl);
						if (fr != FR_OK) {
							disp_fr_err(fr);
						}
						disp_msg_p(msg_error_str,PSTR("WRITE FILE"));
						break;
					}
					block_cntr++;
					if (block_cntr == 0) {
						reset_state_after_error_or_end();
						f_close(&fhdl);
						disp_msg_p(msg_info_saved_str,PSTR("MAX BLOCKS!"));
					}
					break;
				}
				if (buf_idx > MAX_BUF_IDX) {
					reset_state_after_error_or_end();
					f_close(&fhdl);
					disp_msg_p(msg_error_str,PSTR("BLKSIZE EXCEEDED"));
				}
			}
			break;
		}
		case RECV_BIT_TIMEOUT: {
			if (is_in_recv_state) {
				reset_state_after_error_or_end();
				f_close(&fhdl);
				disp_msg_p(msg_info_saved_str,PSTR(""));
			}
			break;
		}
		default:
		case RECV_HANDLER_ACK: {
			break;
		}
	}
	recv_byte = 0;
	recv_state = RECV_HANDLER_ACK;
}
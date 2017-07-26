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
#include <util/atomic.h>
#include <stddef.h>
#include <stdbool.h>
#include "kc_cass_common.h"
#include "kc_cass_send_file.h"
#include "display_util.h"
#include "../ff_avr/xitoa.h"
#include "../lcd/lcd.h"
#include "kc_cass_recv_file.h"

// Clock_Timer1(Prescaler=8MHz/64=125kHz -> 1 tick = 8탎
// f_One=1000Hz -> 1ms/2 => 500탎
// f_Zero=2000Hz -> 500탎/2 => 250탎
// f_Space=500Hz -> 2ms/2 => 1000탎
// all values in RECV_TIMER_... in 탎!
#define RECV_TIMER_TIMEOUT_CNT 160000U			// 160ms
#define RECV_TIMER_ZERO_LOWER_THRESHOLD 120U
#define RECV_TIMER_ZERO_UPPER_THRESHOLD 375U
#define RECV_TIMER_ONE_LOWER_THRESHOLD RECV_TIMER_ZERO_UPPER_THRESHOLD
#define RECV_TIMER_ONE_UPPER_THRESHOLD 750U		// KC85/3 has space cycles of as low as 660탎!
#define RECV_TIMER_SPACE_LOWER_THRESHOLD RECV_TIMER_ONE_UPPER_THRESHOLD

#define IS_SPACE(ctr) (ctr > (RECV_TIMER_SPACE_LOWER_THRESHOLD/8))
#define IS_ONE(ctr) (ctr > (RECV_TIMER_ONE_LOWER_THRESHOLD/8) && ctr <= (RECV_TIMER_ONE_UPPER_THRESHOLD/8))
#define IS_ZERO(ctr) (ctr > (RECV_TIMER_ZERO_LOWER_THRESHOLD/8) && ctr <= (RECV_TIMER_ZERO_UPPER_THRESHOLD/8))

// Timer config
// TIMSK1 = _BV(OCIE1A); // allow interrupts for OCR1A match
// TIMSK1 = _BV(TOIE1); // allow TIMER1 Overflow
// TCCR1B = _BV(WGM12) | _BV(CS11); // Enable timer in CTC-Mode (WGM12), pre-scaler = 8 (CS11), 1MHz timer clock
// TCCR1B = _BV(CS11) | _BV(CS10); // Enable timer in Normal-Mode, TOP=MAX, pre-scaler = 64 (CS11,CS10), 125kHz timer clock
// TCCR1B = _BV(WGM12) | _BV(CS11) | _BV(CS10); // Enable timer in CTC-Mode (WGM12), pre-scaler = 64 (CS11,CS10), 125kHz timer clock
#define START_TIMER1 {\
	TCCR1B = 0; \
	TCNT1 = 0; \
	TIMSK1 = _BV(OCIE1A); \
	TCCR1B = _BV(WGM12) | _BV(CS11) | _BV(CS10); \
}


// TIMSK1 = 0; // disallow all interrupts for timer1
// TCCR1B = 0; // stop timer
// TIFR1 = 0; // clear all interrupt flags
#define STOP_TIMER1 { \
	TIMSK1 = 0; \
	TCCR1B = 0; \
	TIFR1 = 0; \
}

#define MIN_ONE_BITS_IN_VORTON (VORTON_BLOCK / 2U)
#define IS_FILE_START_VORTON(ctr) (ctr >= (VORTON_FFBLOCK + 10U))
#define IS_BLOCK_VORTON(ctr) (ctr >= MIN_ONE_BITS_IN_VORTON)
#define MIN_SPACE_CNT_FOR_SENDFILE 100U
#define IS_START_SENDFILE(ctr) (ctr >= MIN_SPACE_CNT_FOR_SENDFILE)

#define DISK_BLOCK_SIZE (DATA_BUF_SIZE - 1)
#define MAX_BUF_IDX (DATA_BUF_SIZE - 1)

typedef enum {
	RECV_BLOCK_VORTON_DETECTED,
	RECV_FILE_START_VORTON_DETECTED,
	RECV_BYTE_READY, 
	RECV_BIT_TIMEOUT, 
	RECV_HANDLER_ACK, 
	RECV_OVERRUN_OCCURED,
	RECV_START_SENDFILE
} recv_state_enum;
volatile recv_state_enum recv_state = RECV_HANDLER_ACK;

volatile uint8_t buf_idx = 0;
char file_name_on_disk[LEN_DATEINAME + 1 + LEN_DATEITYP + 1]; // contains 8byte name + 1 dot + 3byte ext + 0x00byte
char file_type[LEN_DATEITYP + 1]; // 3byte + 0x00byte
const char msg_info_saved_str[] PROGMEM = "INFO: FILE SAVED";

volatile bool is_time_measure_running = false; // if false ISR(INT0_vect) starts a new time measurement
volatile uint8_t recv_byte = 0;
volatile uint8_t block_cntr = 0;
volatile uint16_t vorton_cntr = 0;
volatile uint16_t space_cntr = 0;
volatile uint16_t cntr_val = 0;

static void reset_recv_state() {
	STOP_TIMER1;
	if (system_state == RECEIVING) {
		system_state = IDLE;
	}
	block_cntr = 0;
	buf_idx = 0;
	file_name_on_disk[0] = 0;
}

// should only be fired when timer is not reset before,
// meaning this is a timeout (doesn't need to be an error,
// can be at end of file)
//ISR(TIMER1_OVF_vect) {
ISR(TIMER1_COMPA_vect) {
	STOP_TIMER1;
	is_time_measure_running = false;
	if (recv_state == RECV_HANDLER_ACK) {
		recv_state = RECV_BIT_TIMEOUT;
	}
	else {
		recv_state = RECV_OVERRUN_OCCURED;
	}
	#ifdef DEBUG_RECV_TIMER
	MONITOR_RECV_PIN1_HIGH;
	#endif
}

// is fired at each change in logical level of pin
ISR(INT0_vect) {
	cntr_val = TCNT1;
	if (is_time_measure_running) {
		START_TIMER1; // need to start it to catch the last timeout
	#ifdef DEBUG_RECV_TIMER
	MONITOR_RECV_PIN1_LOW;
	#endif
		if (IS_SPACE(cntr_val)) {
			if (recv_state != RECV_HANDLER_ACK) {
				recv_state = RECV_OVERRUN_OCCURED;
			}
			else {
				if (IS_START_SENDFILE(space_cntr)) {
					recv_state = RECV_START_SENDFILE;
					space_cntr = 0;
				}
				else {
					// Spaces can be very long (>80ms on KC85/3), this is to filter out noise
					// We want to act only on the first SPACE to avoid saving a block multiple times
					if (space_cntr==0) { 
						if (IS_FILE_START_VORTON(vorton_cntr)) {
							recv_state = RECV_FILE_START_VORTON_DETECTED;
						}
						else if (IS_BLOCK_VORTON(vorton_cntr)) {
							recv_state = RECV_BLOCK_VORTON_DETECTED;
						}
						else {
							recv_state = RECV_BYTE_READY;
						}
					}
					space_cntr++;
				}
			}
			vorton_cntr = 0;
			is_time_measure_running = false;
		}
		else if (IS_ONE(cntr_val)) {
			recv_byte >>= 1;
			vorton_cntr++;
			space_cntr = 0;
			recv_byte |= 0x80;
			is_time_measure_running = false;
		}
		else if (IS_ZERO(cntr_val)) {
			recv_byte >>= 1;
			vorton_cntr=0;
			space_cntr=0;
			is_time_measure_running = false;
		}
		// If we don't recognize one of the signals, we ignore it
		// this is done to filter out noise
	}
	else {
		// initialize and start timer
		START_TIMER1;
	#ifdef DEBUG_RECV_TIMER
	MONITOR_RECV_PIN1_LOW;
	#endif
		is_time_measure_running = true;
	}
}

static void get_filename_from_fcb() {
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
		KC_FCB* fcb = (KC_FCB*) (buf + 1);
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
		strlcpy_P(file_name_on_disk,PSTR(DEFAULT_FILE_NAME),9);
		i=8;
	}
	file_name_on_disk[i] = '.';
	strlcpy_P(file_name_on_disk+i+1,PSTR(FILE_EXT),4);
}

void kc_cass_recv_file_init() {
	#ifdef DEBUG_RECV_TIMER
	MONITOR_RECV_DDR |= (_BV(MONITOR_RECV_BIT1) | _BV(MONITOR_RECV_BIT2));
	MONITOR_RECV_PIN1_LOW;
	MONITOR_RECV_PIN2_LOW;
	#endif
	
	CASS_IN_DDR &= ~_BV(CASS_IN_PIN); // Configure input pin as INPUT
	CASS_IN_PORT &= ~_BV(CASS_IN_PIN); // Disable pullup
	EIFR &= _BV(INT0); // clear int flag reg
	EIMSK |= _BV(INT0); // enable int INT0
	EICRA = _BV(ISC00); // any change in logical level on pin will generate int
	OCR1A = RECV_TIMER_TIMEOUT_CNT/8U; // set TOP of counter
	TCCR1A = 0; // COM1[A,B][1,0]=0 => Compare Output Mode=normal; WGM[10,11]=0 => Normal or CTC mode
	reset_recv_state();
}

void kc_cass_recv_file_disable() {
	EIMSK &= ~_BV(INT0); // make sure INT0 pin interrupts are disabled
	CASS_IN_DDR &= ~_BV(CASS_IN_PIN); // Configure input pin as INPUT
	reset_recv_state();
}

/************************************************************************/
/* To be called from the main while loop                                */
/************************************************************************/
void kc_cass_handle_recv_file() {
	recv_state_enum _recv_state;
	uint8_t _recv_byte;
	ATOMIC_BLOCK(ATOMIC_RESTORESTATE) {
		_recv_state = recv_state;
		recv_state = RECV_HANDLER_ACK;
		_recv_byte = recv_byte;
	}
	switch (_recv_state) {
		case RECV_OVERRUN_OCCURED: {
			if (system_state == RECEIVING) {
				reset_recv_state();
				f_close(&fhdl);
				disp_msg_p(msg_error_str,PSTR("OVERRUN!")); // we are only interested in overruns during receive
			}
			break;
		}
		case RECV_FILE_START_VORTON_DETECTED: {
			if (system_state == IDLE) {
				system_state = RECEIVING;
				block_cntr = 0;
				memset(buf,0x0,DATA_BUF_SIZE);
				buf_idx = 0;
				display_recvinfo(NULL,0,NULL);
			}
			break;
		}
		case RECV_BLOCK_VORTON_DETECTED: {
			if (system_state == RECEIVING) {
				memset(buf,0x0,DATA_BUF_SIZE);
				buf_idx = 0;
			}
			break;
		}
		case RECV_BIT_TIMEOUT:
		case RECV_BYTE_READY: {
			if (system_state == RECEIVING) {
				if (buf_idx < MAX_BUF_IDX) {
					buf[buf_idx] = _recv_byte;
					buf_idx++;
				}
				else if (buf_idx == MAX_BUF_IDX) { // block completely received
					UINT bytes_written = 0;
					if (_recv_byte != calculate_checksum()) {
						reset_recv_state();
						f_close(&fhdl);
						lcd_clrscr();
						xprintf(PSTR("WRONG CHECKSUM"));
						lcd_gotoxy(0,1);
						xprintf(PSTR("rcv=%d clc=%d"), _recv_byte, calculate_checksum());
						_delay_ms(ERROR_DISP_MILLIS);
						break;
					}

					#ifdef DEBUG_RECV_TIMER
					MONITOR_RECV_PIN2_HIGH;
					#endif
					if (block_cntr == 0) { // first block
						f_close(&fhdl); // just to be sure
						get_filename_from_fcb();
						
						fr = f_open(&fhdl, file_name_on_disk, FA_WRITE | FA_CREATE_ALWAYS);
						if (fr != FR_OK) {
							reset_recv_state();
							f_close(&fhdl);
							disp_fr_err(fr);
							disp_msg_p(msg_error_str,PSTR("OPEN FOR WRITE"));
							break;
						}
						display_recvinfo(file_name_on_disk,buf[0],file_type);
						
						char lbuf[TAP_HEADER_LEN]; //we skip the final 0x0
						strncpy_P(lbuf,tap_header_str,TAP_HEADER_LEN);
						
						fr = f_write(&fhdl,lbuf, TAP_HEADER_LEN, &bytes_written);
						if (fr != FR_OK || bytes_written != TAP_HEADER_LEN) {
							reset_recv_state();
							f_close(&fhdl);
							if (fr != FR_OK) {
								disp_fr_err(fr);
							}
							disp_msg_p(msg_error_str,PSTR("WRITE FILE HDR"));
							break;
						}
					}
					else {
						display_upd_recvinfo(buf[0]);
					}

					fr = f_write(&fhdl, buf, DISK_BLOCK_SIZE, &bytes_written);
					#ifdef DEBUG_RECV_TIMER
					MONITOR_RECV_PIN2_LOW;
					#endif
					if (fr != FR_OK || bytes_written != DISK_BLOCK_SIZE) {
						reset_recv_state();
						f_close(&fhdl);
						if (fr != FR_OK) {
							disp_fr_err(fr);
						}
						disp_msg_p(msg_error_str,PSTR("WRITE FILE"));
						break;
					}
					// stop the receiving process if we are in a TIMEOUT state
					// this is the intended exit of the receiving process
					if (_recv_state == RECV_BIT_TIMEOUT) {
						reset_recv_state();
						f_close(&fhdl);
						#ifdef DEBUG_RECV_TIMER
						MONITOR_RECV_PIN1_LOW;
						#endif
						disp_msg_p(msg_info_saved_str,PSTR(""));
						display_by_name(file_name_on_disk, false);
					}
					block_cntr++;
					if (block_cntr == 0) {
						reset_recv_state();
						f_close(&fhdl);
						disp_msg_p(msg_info_saved_str,PSTR("MAX BLOCKS!"));
						break;
					}
				}
				else { //  if (buf_idx > MAX_BUF_IDX)
					reset_recv_state();
					f_close(&fhdl);
					disp_msg_p(msg_error_str,PSTR("BLKSIZE EXCEEDED"));
					break;
				}
			}
			break;
		}
		case RECV_START_SENDFILE: {
			send_file(&Finfo);
			break;
		}
		default:
		case RECV_HANDLER_ACK: {
			break;
		}
	}
	#ifdef DEBUG_RECV_TIMER
	MONITOR_RECV_PIN2_LOW;
	#endif
}
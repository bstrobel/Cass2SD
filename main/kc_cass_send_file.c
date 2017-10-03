#include <avr/io.h>
#include <avr/interrupt.h>
#include <avr/sfr_defs.h>
#include <stdbool.h>
#include "../ff_avr/xitoa.h"
#include "../ff_avr/ff.h"
#include "../ff_avr/diskio.h"
#include "../lcd/lcd.h"

#include "kc_cass_common.h"
#include "display_util.h"
#include "debounced_keys.h"
#include "kc_cass_recv_file.h"
#include "kc_cass_send_file.h"

volatile SEND_STATE send_state = DONE;

#define SEND_TIMER_PRESCALER 64UL
#define SEND_TIMER_TICK_USEC ((SEND_TIMER_PRESCALER * 1000000UL) / F_CPU)  // 4탎ec for 16MHz
#define SEND_TIMER_SPACE_USEC (1800UL / 2UL) // Space period time Z9001 1750탎ec, KC85/3 1820탎ec
#define SEND_TIMER_ONE_USEC (950UL / 2UL) // One period time Z9001 920탎ec, KC85/3 980탎ec
#define SEND_TIMER_ZERO_USEC (520UL / 2UL) // Zero period time Z9001 500탎ec, KC85/3 540탎ec

#define OCR_SPACE_SENDFILE ((SEND_TIMER_SPACE_USEC / SEND_TIMER_TICK_USEC) - 1UL) // 224 for 16MHz/64
#define OCR_BIT_ONE_SENDFILE ((SEND_TIMER_ONE_USEC / SEND_TIMER_TICK_USEC) - 1UL) // 117 for 16MHz/64
#define OCR_BIT_ZERO_SENDFILE ((SEND_TIMER_ZERO_USEC / SEND_TIMER_TICK_USEC) - 1UL) // 64 for 16MHz/64

static void config_and_start_timer_sendfile(uint8_t ocr_val) {
	OCR0A = ocr_val;
	TCNT0 = 0; // reset timer
	TIFR0 = 0; // clear all INT flags
	TCCR0A = _BV(WGM01); // CTC mode -> TOP is OCR0A
	TIMSK0 = _BV(OCIE0A); // allow OCA interrupt generation
	// timer is started with the next command!
	#if (SEND_TIMER_PRESCALER == 64U)
		TCCR0B = _BV(CS01)|_BV(CS00); // 64 prescaler -> 250kHz, 4탎/tick for F_CPU=16MHz
	#else
		#error "Prescaler initialization for TIMER0 needs to be adjusted!"
	#endif
}

ISR(TIMER0_COMPA_vect) {
	#ifdef DEBUG_TIMER
	MONITOR_PORT |= _BV(MONITOR_BIT);
	#endif
	switch(send_state) {
		case FIRST_HALF:
			CASS_OUT_PORT &= ~(_BV(CASS_OUT_PIN));
			send_state = SECOND_HALF;
			break;
		case SECOND_HALF:
		case DONE:
			send_state = DONE;
			TCCR0B = 0; // stop timer
			TCNT0 = 0;
			TIMSK0 = 0; //disallow Timer0 interrupts
			break;
		default:
			break;
	}
	#ifdef DEBUG_TIMER
	MONITOR_PORT &= ~_BV(MONITOR_BIT);
	#endif
}

void kc_cass_send_file_init() {
	CASS_OUT_DDR |= _BV(CASS_OUT_PIN); // Configure output pin as OUTPUT
	CASS_OUT_PORT &= ~(_BV(CASS_OUT_PIN)); // Initialize output pin to LOW

	#ifdef DEBUG_TIMER
	MONITOR_DDR |= _BV(MONITOR_BIT);
	MONITOR_PORT &= ~_BV(MONITOR_BIT);
	#endif
}


// sends the specified bit using Timer0
// blocks until the bit is sent only if bit_type!=SPACE
// this gives us time to reload buffer and do computation while sending the space
static void send_bit(BIT_TYPE bit_type) {
	#ifdef DEBUG_TIMER
	MONITOR_PIN |= MONITOR_BIT;
	#endif
	send_state = FIRST_HALF;
	uint8_t ocr_val;
	switch(bit_type) {
		case SPACE:
			ocr_val = OCR_SPACE_SENDFILE;
			break;
		case ONE:
			ocr_val = OCR_BIT_ONE_SENDFILE;
			break;
		default:
		case ZERO:
			ocr_val = OCR_BIT_ZERO_SENDFILE;
			break;
	}
	CASS_OUT_PORT |= _BV(CASS_OUT_PIN);
	config_and_start_timer_sendfile(ocr_val);
		// block until bit is sent
	while (bit_type != SPACE && send_state != DONE);
	#ifdef DEBUG_TIMER
	MONITOR_PIN |= MONITOR_BIT;
	#endif
}

// sends out a byte
// LSB first!
static void send_byte(uint8_t byte) {
	// wait until the previous SPACE is completed
	while (send_state != DONE);
	uint8_t mask = 1;
	for (uint8_t i = 0; i < 8; i++) {
		if (mask & byte)
			send_bit(ONE);
		else
			send_bit(ZERO);
		mask <<= 1;
	}
	send_bit(SPACE); // this doesn't block!
}

static void reset_state() {
	kc_cass_recv_file_init();
	system_state = IDLE;
}

void send_file(FILINFO* Finfo) {
	if (system_state != IDLE) {
		return;
	}
	system_state = SENDING;
	kc_cass_recv_file_disable();
	select_key_changed = false;
	uint8_t current_block = 1;
	if (load_first_block_and_check_type(Finfo)) {
		
		display_sendinfo(Finfo->fname,block_len,number_of_blocks,kc_file_type);

		while(1) {
			buf[129] = calculate_checksum();

			// Send "Vorton"
			int num_vorton = VORTON_BLOCK;
			if (current_block == 1) {
				num_vorton = VORTON_BEGIN;
			} else if (current_block == number_of_blocks) {
				num_vorton = VORTON_FFBLOCK;
			}
			display_upd_sendinfo(buf[0]);
			while(send_state != DONE); // wait for the previous SPACE to finish
			for (int i = 0; i < num_vorton; i++) {
				send_bit(ONE);
			}
			send_bit(SPACE);
			
			// send the block
			for (uint8_t i = 0; i < DATA_BUF_SIZE; i++) {
				send_byte(buf[i]);
			
				if (select_key_changed && select_key_pressed) {
					select_key_changed = false;
					disp_msg_p(PSTR("SEND FILE"),PSTR("INTERRUPTED!"));
					f_close(&fhdl);
					reset_state();
					display_fileinfo(Finfo);
					return;
				}
			}
				
			if (current_block == number_of_blocks) { // send the final SPACE
				send_bit(SPACE);
				while(send_state != DONE);
				CASS_OUT_PORT |= _BV(CASS_OUT_PIN); // final L->H edge
				break; // we are done -> exit while loop				
			}
			UINT bytes_read;
			// read the next block
			if (disp_fr_err(f_read(&fhdl, start_buf_ptr, block_len, &bytes_read))) {
				disp_msg_p(msg_error_str,PSTR("EOF too early!"));
				break;
			}
			if (bytes_read != block_len) {
				disp_msg_p(msg_error_str,msg_block_too_short_str);
				break;
			}
			// generate next blocknr if it doesnt come from the file
			if (HAS_NO_BLOCKNR) {
				buf[0]++;
				if (buf[0] == (number_of_blocks - 1) && kc_file_type != BASIC_NO_HEADER && kc_file_type != BASIC_W_HEADER)
					buf[0]=0xff;
			}
			// Skip 0xff block for TAP_BASIC_EXTRA_BLOCKS
			if (kc_file_type == TAP_BASIC_EXTRA_BLOCKS && buf[0] == 0xff) {
				buf[0] = number_of_blocks;
			}
			current_block++;
		}
		f_close(&fhdl);
	}
	display_fileinfo(Finfo);
	while(send_state!=DONE);
	reset_state();
}
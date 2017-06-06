#include <avr/io.h>
#include <avr/interrupt.h>
#include <avr/sfr_defs.h>
#define __PROG_TYPES_COMPAT__
#include <avr/pgmspace.h>
#include <string.h>
#include <util/delay.h>
#include <stdbool.h>
#include "../ff_avr/xitoa.h"
#include "../ff_avr/ff.h"
#include "../ff_avr/diskio.h"
#include "../lcd/lcd.h"

#include "kc_cass_send_file.h"
#include "kc_cass_format_def.h"
#include "display_util.h"

typedef enum {SPACE=0, ONE, ZERO} BIT_TYPE;

#define SEND_BUF_SIZE 130
uint8_t buf[SEND_BUF_SIZE]; //send buffer
uint8_t* start_buf_ptr = buf;

volatile SEND_STATE send_state = DONE;

FIL fhdl;
FRESULT fr;

#define OCR_SPACE_SENDFILE (F_CPU / 64 / 571 / 2 - 1) //108
#define OCR_BIT_ONE_SENDFILE (F_CPU / 64 / 1087 / 2 - 1) //56
#define OCR_BIT_ZERO_SENDFILE (F_CPU / 64 / 2000 / 2 - 1) //30

static void config_and_start_timer_sendfile(uint8_t ocr_val) {
	OCR0A = ocr_val;
	TCNT0 = 0; // reset timer
	TIFR0 = 0; // clear all INT flags
	TCCR0A = _BV(WGM01); // CTC mode -> TOP is OCR0A
	TIMSK0 = _BV(OCIE0A); // allow OCA interrupt generation
	// timer is started with the next command!
	TCCR0B = _BV(CS01)|_BV(CS00); // 64 prescaler -> 125000Hz, 8µs/tick for F_CPU=8MHz
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

UINT bytes_read = 0;
uint8_t block_len = 128;
bool has_checksum = false;
KC_FILE_TYPE kc_file_type = RAW;

static bool check_for_blocknrs() {
	// rewind to start of file
	if (disp_fr_err(f_lseek(&fhdl, 0))) {
		f_close(&fhdl);
		return false;
	}

	bool first_block = true;
	uint8_t last_blocknr = 0;
	// Sweep through file and see if blocknumbers are consecutive
	while (bytes_read > 0 || first_block) {
		if (disp_fr_err(f_read(&fhdl, buf, 129, &bytes_read))) {
			f_close(&fhdl);
			return false;
		}
		if (first_block) {
			if (buf[0] < 2)
				// Normal files start with block=0, BASIC files with block=1
				last_blocknr = buf[0];
			else {
				// already the first blocknr is wrong, so we dont have blocknrs
				start_buf_ptr = buf + 1;
				block_len = 128;
				break;
			}
		}
		else {
			if (buf[0] == last_blocknr + 1)
				last_blocknr = buf[0];
			else {
				if (buf[0] != 0xff) {
					// the last blocknr is always 0xff
					start_buf_ptr = buf;
					block_len = 129;
				}
				else {
					// no consecutive blocknr -> we assume no blocknrs
					start_buf_ptr = buf + 1;
					block_len = 128;
				}
				break;
			}
		}

	}
	// rewind again
	if (disp_fr_err(f_lseek(&fhdl, 0))) {
		f_close(&fhdl);
		return false;
	}
	return true;
}

static bool check_non_tap_types(FILINFO* Finfo) {
	if(!check_for_blocknrs())
		return false;

	// load the first block completely
	// start_buf_ptr and block_len are now correct
	if (disp_fr_err(f_read(&fhdl, start_buf_ptr, block_len, &bytes_read))) {
		f_close(&fhdl);
		return false;
	}
	// check if BASIC header

	KC_FCB_BASIC* fcb_basic = (KC_FCB_BASIC*) start_buf_ptr;
	uint8_t b1 = fcb_basic->dateityp[0];
	uint8_t b2 = fcb_basic->dateityp[1];
	uint8_t b3 = fcb_basic->dateityp[2];
	if ( b1 >= 0xd3 && b1 <= 0xd9 && b1 == b2 && b1 == b3) {
		// BASIC header found
		kc_file_type = BASIC;
		return true;
	}

	char* ext = strchr(Finfo->fname,'.');

	// check for extension SSS -> headerless BASIC files
	if (!strcmp_P(ext+1,PSTR("SSS"))) {
		kc_file_type = BASIC;
		// Create BASIC header in the buffer
		*start_buf_ptr = 0xd3;
		*(start_buf_ptr+1) = 0xd3;
		*(start_buf_ptr+2) = 0xd3;
		// Copy the filename in the header
		for (uint8_t i=3; i < 11; i++) {
			if (Finfo->fname + i < ext)
				start_buf_ptr[i] = Finfo->fname[i];
			else
				start_buf_ptr[i] = 0x20;
		}
		// we have to reload since we've overwritten some data
		if (disp_fr_err(f_lseek(&fhdl, 0))) {
			f_close(&fhdl);
			return false;
		}
		// load the data from the file after the header we just created
		if (disp_fr_err(f_read(&fhdl, start_buf_ptr + 11, block_len - 11, &bytes_read))) {
			f_close(&fhdl);
			return false;
		}
		return true;			
	}

	// check if MC header
	KC_FCB_MC* fcb_mc = (KC_FCB_MC*) start_buf_ptr;
	kc_file_type = MACHINE_CODE;
	for (uint8_t i=0; i<8; i++) {
		if ((fcb_mc->dateiname[i] < 0x21 || fcb_mc->dateiname[i] > 0x7f) && fcb_mc->dateiname[i] != 0) {
			// found unprintable character -> cannot be a real fcb
			kc_file_type = RAW;
			break;
		}
	}

	if (kc_file_type == MACHINE_CODE) {
		// we still believe in MC type
		for (uint8_t i=0; i<3; i++) {
			if ((fcb_mc->dateityp[i] < 0x21 || fcb_mc->dateityp[i] > 0x7f) && fcb_mc->dateityp[i] != 0) {
				// found unprintable character -> cannot be a real fcb
				kc_file_type = RAW;
				break;
			}
		}
	}

	// still RAW, we have to create a FCB
	if (kc_file_type == RAW) {
		memset(start_buf_ptr,0x0,block_len);
		// we reuse the fcb_mc pointer created earlier
		for (uint8_t i=0; i<8; i++) {
			if (Finfo->fname + i < ext)
				fcb_mc->dateiname[i] = Finfo->fname[i];
			else
				fcb_mc->dateiname[i] = 0x20;
		}
		bool end_reached = false;
		for (uint8_t i=0; i<3; i++) {
			if (!end_reached && ext[i+1])
				fcb_mc->dateityp[i] = ext[i+1];
			else
			{
				end_reached=true;
				fcb_mc->dateiname[i] = 0x20;
			}
		}
		fcb_mc->aadr = 0x300;
		// we assume here that a RAW file doesn't have block numbers
		fcb_mc->eadr = 0x300 + Finfo->fsize - 128;
		fcb_mc->sadr = 0xffff;
	}
	return true;
}

static bool load_first_block_and_check_type(FILINFO* Finfo) {
	block_len = 129;
	start_buf_ptr = buf;
	if (Finfo->fsize < 128) {
		lcd_clrscr();
		xprintf(PSTR("ERR:%s"),Finfo->fname);
		lcd_gotoxy(0,1);
		xprintf(PSTR("Too short:%ub"),Finfo->fsize);
		_delay_ms(ERROR_DISP_MILLIS);
		return false;
	}
	if (disp_fr_err(f_open(&fhdl, Finfo->fname, FA_READ | FA_OPEN_EXISTING))) {
		f_close(&fhdl);
		return false;
	}
	// load first 16bytes to check for TAP header
	if (disp_fr_err(f_read(&fhdl, buf, TAP_HEADER_LEN, &bytes_read))) {
		f_close(&fhdl);
		return false;
	}

	if (strncmp_P((char*)buf,tap_header_str,bytes_read)) {
		if(!check_non_tap_types(Finfo))
			return false;
	}
	else {

		// it is definitely TAP format which always has blocknumbers
		// and doesnt need any other special recognition procedure
		kc_file_type = TAP;

		// workaround for TAP files that contain a meaningless block 0 before the
		// actual first BASIC block #1. Z9001 doesn't like that.
		// so load the first block after the header and see if it is BASIC
		if (disp_fr_err(f_lseek(&fhdl, TAP_HEADER_LEN + 129)) || 
			disp_fr_err(f_read(&fhdl, start_buf_ptr, block_len, &bytes_read))) {
			f_close(&fhdl);
			return false;
		}
		KC_FCB_BASIC* fcb_basic = (KC_FCB_BASIC*) (start_buf_ptr + 1);
		uint8_t b1 = fcb_basic->dateityp[0];
		uint8_t b2 = fcb_basic->dateityp[1];
		uint8_t b3 = fcb_basic->dateityp[2];
		if ( b1 >= 0xd3 && b1 <= 0xd9 && b1 == b2 && b1 == b3) {
			// BASIC header found
			kc_file_type = TAP_BASIC;
		}
		else
		{
			// it is not BASIC
			// rewind to beginning of first block after TAP header and reload
			if (disp_fr_err(f_lseek(&fhdl, TAP_HEADER_LEN)) ||
				disp_fr_err(f_read(&fhdl, start_buf_ptr, block_len, &bytes_read))) {
				f_close(&fhdl);
				return false;
			}
		}
		return true;
	}
	return true;
}

void send_file(FILINFO* Finfo) {
	if (load_first_block_and_check_type(Finfo)) {
		// block# handling
		uint8_t num_of_last_block = (uint8_t)(Finfo->fsize / 128) - 1;
		
		// BASIC files start with block# 1!
		if (block_len == 128) {
			if (kc_file_type == BASIC) {
				buf[0] = 1;
				num_of_last_block++;
			}
			else {
				buf[0] = 0;
			}
		}

		display_sendinfo(Finfo->fname,block_len,num_of_last_block,kc_file_type);

		while(1) {
			// calculate checksum
			buf[129] = 0;
			for (int i = 1; i < 129; i++)
				buf[129] += buf[i];
				
			// Send "Vorton"
			int num_vorton = 0;
			switch (buf[0]) {
				case 0:
					num_vorton = VORTON_BEGIN;
					break;
				case 1:
					if (kc_file_type == TAP_BASIC || kc_file_type == BASIC)
						num_vorton = VORTON_BEGIN;
					else
						num_vorton = VORTON_BLOCK;
					break;
				case 0xff:
					num_vorton = VORTON_FFBLOCK;
					break;
				default:
					num_vorton = VORTON_BLOCK;
					break;
			}
			display_upd_sendinfo(buf[0]);
			
			while(send_state != DONE); // wait for the previous SPACE to finish
			for (int i = 0; i < num_vorton; i++)
				send_bit(ONE);
			send_bit(SPACE);
			
			// send the block
			for (uint8_t i = 0; i < SEND_BUF_SIZE; i++)
				send_byte(buf[i]);
				
			if (buf[0] == 0xff) {
				send_bit(SPACE);
				if (kc_file_type != BASIC || kc_file_type != TAP_BASIC) {
					while(send_state != DONE);
					CASS_OUT_PORT |= _BV(CASS_OUT_PIN); // final L->H edge not for BASIC
				}
				break; // exit while loop				
			}
			// read the next block
			if (disp_fr_err(f_read(&fhdl, start_buf_ptr, block_len, &bytes_read)))
				break;
			if (bytes_read != block_len) {
				disp_err("ERROR:","Blk too short!");
				break;
			}
			// generate next blocknr if it doesnt come from the file
			if (block_len==128) {
				buf[0]++;
				if (buf[0] == num_of_last_block)
					buf[0]=0xff;
			}
		}
		f_close(&fhdl);
	}
}
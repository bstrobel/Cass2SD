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
uint8_t number_of_blocks = 0;

/************************************************************************/
/* Scans the file and checks if there are consecutive blocknrs          */
/* sets block_len=128, start_buf_prt = buf+1 if file has no blocknrs    */
/* sets block_len=129, start_buf_prt = buf if file provides blocknrs    */
/************************************************************************/
static bool detect_block_len() {
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

static bool check_basic_fcb() {
	KC_FCB_BASIC* fcb_basic = (KC_FCB_BASIC*) (buf + 1);
	uint8_t b1 = fcb_basic->dateityp[0];
	uint8_t b2 = fcb_basic->dateityp[1];
	uint8_t b3 = fcb_basic->dateityp[2];
	return ( b1 >= 0xd3 && b1 <= 0xd9 && b1 == b2 && b1 == b3);
}

static bool check_non_tap_types(FILINFO* Finfo) {
	// non-TAP files may have the block numbers in the file or not
	// this determines the block_len:
	//  129 with blocknr
	//  128 without blocknr
	if(!detect_block_len()) {
		return false;
	}

	// load the first block completely
	// start_buf_ptr and block_len are now correct
	if (disp_fr_err(f_read(&fhdl, start_buf_ptr, block_len, &bytes_read))) {
		f_close(&fhdl);
		return false;
	}

	// check if BASIC FCB
	if (check_basic_fcb()) {
		// BASIC FCB found
		kc_file_type = BASIC_W_HEADER;
		number_of_blocks = (uint8_t)(Finfo->fsize / block_len);
		if (block_len == 128) {
			buf[0] = 1; // BASIC files start with block #1
		}
		return true;
	}

	// check the file extension if it is of BASIC type ('SSS','TTT',...)
	char* ext = strchr(Finfo->fname,'.') + 1;
	for (uint8_t c = 0x53; c <= 0x59; c++) { // stepping through from 'S' to 'Y'
		if (ext[0] == c && ext[1] == c && ext[2] == c) {
			kc_file_type = BASIC_NO_HEADER; // -> we found a BASIC file ext.
			// Create the BASIC FCB
			start_buf_ptr[0] = c + 0x80; // set the magic 3 letter header (0xd3d3d3, 0xd4d4d4, ...)
			start_buf_ptr[1] = c + 0x80;
			start_buf_ptr[2] = c + 0x80;
			// Copy the filename in the header
			for (uint8_t i=0; i < 8; i++) {
				if (Finfo->fname + i < (ext - 1)) {
					start_buf_ptr[i+3] = Finfo->fname[i];
				}
				else {
					start_buf_ptr[i+3] = 0x20;
				}
			}
			// reload the file contents after the just created header
			if (disp_fr_err(f_lseek(&fhdl, 0))) {
				f_close(&fhdl);
				return false;
			}
			uint8_t bytes_to_read = block_len - BASIC_HEADER_LEN;
			if (disp_fr_err(f_read(&fhdl, start_buf_ptr + BASIC_HEADER_LEN, bytes_to_read, &bytes_read))) {
				f_close(&fhdl);
				return false;
			}
			if (bytes_read != bytes_to_read) {
				disp_err("ERROR:","Blk too short!");
				return false;
			}
			number_of_blocks = (uint8_t)((Finfo->fsize + BASIC_HEADER_LEN) / block_len);
			if (block_len == 128) {
				buf[0] = 1; // BASIC files start with block #1
			}
			return true;
		}
	}

	if (block_len == 128) {
		buf[0] = 0; // initialize the blocknr if not provided
	}
	
	// No BASIC FCB and no BASIC extension, now check for MC header
	KC_FCB_MC* fcb_mc = (KC_FCB_MC*) (buf + 1);
	kc_file_type = MACHINE_CODE;
	for (uint8_t i=0; i<8; i++) {
		if ((fcb_mc->dateiname[i] < 0x21 || fcb_mc->dateiname[i] > 0x7f) && fcb_mc->dateiname[i] != 0) {
			// found unprintable character -> cannot be a real fcb
			kc_file_type = RAW;
			break;
		}
	}

	if (kc_file_type == MACHINE_CODE) {
		// Filename is ok, now we check the file extension
		for (uint8_t i=0; i<3; i++) {
			if ((fcb_mc->dateityp[i] < 0x21 || fcb_mc->dateityp[i] > 0x7f) && fcb_mc->dateityp[i] != 0) {
				// found unprintable character -> cannot be a real fcb
				kc_file_type = RAW;
				break;
			}
		}
	}

	// it is RAW, we have to create a FCB
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
		number_of_blocks = (uint8_t)((Finfo->fsize / block_len) + 1);
		if (block_len == 128) {
			fcb_mc->eadr = 0x300 + Finfo->fsize;
		}
		else {
			fcb_mc->eadr = 0x300 + (Finfo->fsize - number_of_blocks);
		}
		fcb_mc->sadr = 0xffff;
	}
	else { // it is MACHINE_CODE with FCB
		number_of_blocks = (uint8_t)(Finfo->fsize / block_len);
	}
	return true;
}

static bool check_tap_types(FILINFO* Finfo) {
	// it is definitely TAP format which always has blocknumbers
	// and doesnt need any other special recognition procedure
	kc_file_type = TAP;
	
	number_of_blocks = (uint8_t)((Finfo->fsize - TAP_HEADER_LEN) / 129);
	
	// read the first block after the TAP_HEADER
	if (disp_fr_err(f_lseek(&fhdl, TAP_HEADER_LEN)) || disp_fr_err(f_read(&fhdl, start_buf_ptr, block_len, &bytes_read))) {
		f_close(&fhdl);
		return false;
	}

	if (check_basic_fcb()) {
		kc_file_type = TAP_BASIC;
	} else {
		// workaround for TAP files that contain a meaningless block 0 before the
		// actual first BASIC block #1. Z9001 doesn't like that.
		// so load the first block after the header and see if it is BASIC
		if (disp_fr_err(f_read(&fhdl, start_buf_ptr, block_len, &bytes_read))) {
			f_close(&fhdl);
			return false;
		}
		if (check_basic_fcb()) {
			// BASIC header found
			kc_file_type = TAP_BASIC_EXTRA_BLOCKS;
			number_of_blocks = (uint8_t)(((Finfo->fsize - TAP_HEADER_LEN) / 129) - 1);
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
	}
	return true;
}

static bool load_first_block_and_check_type(FILINFO* Finfo) {
	block_len = 129;
	start_buf_ptr = buf;
	if (Finfo->fsize < 128) { // file too short -> we don't even care to look further
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
	// load first TAP_HEADER_LEN to check for TAP header
	if (disp_fr_err(f_read(&fhdl, buf, TAP_HEADER_LEN, &bytes_read))) {
		f_close(&fhdl);
		return false;
	}

	if (strncmp_P((char*)buf,tap_header_str,bytes_read)) {
		return check_non_tap_types(Finfo);
	}
	else {
		return check_tap_types(Finfo);
	}
	return true;
}

void send_file(FILINFO* Finfo) {
	if (load_first_block_and_check_type(Finfo)) {
		
		display_sendinfo(Finfo->fname,block_len,number_of_blocks,kc_file_type);

		while(1) {
			// calculate checksum
			buf[129] = 0;
			for (uint8_t i = 1; i < 129; i++)
				buf[129] += buf[i];
				
			// Send "Vorton"
			int num_vorton = 0;
			switch (buf[0]) {
				case 0:
					num_vorton = VORTON_BEGIN;
					break;
				case 1:
					if (kc_file_type == TAP_BASIC || kc_file_type == TAP_BASIC_EXTRA_BLOCKS || kc_file_type == BASIC_NO_HEADER || kc_file_type == BASIC_W_HEADER)
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
				
			if (
				(
					(
						kc_file_type == BASIC_NO_HEADER || 
						kc_file_type == BASIC_W_HEADER || 
						kc_file_type == TAP_BASIC || 
						kc_file_type == TAP_BASIC_EXTRA_BLOCKS
					)
					&& buf[0] == number_of_blocks
				)
				|| buf[0] == 0xff) { // send the final SPACE
				send_bit(SPACE);
				while(send_state != DONE);
				CASS_OUT_PORT |= _BV(CASS_OUT_PIN); // final L->H edge
				break; // we are done -> exit while loop				
			}
			// read the next block
			if (disp_fr_err(f_read(&fhdl, start_buf_ptr, block_len, &bytes_read))) {
				disp_err("ERROR","EOF too early!");
				break;
			}
			if (bytes_read != block_len) {
				disp_err("ERROR:","Blk too short!");
				break;
			}
			// generate next blocknr if it doesnt come from the file
			if (block_len==128) {
				buf[0]++;
				if (buf[0] == (number_of_blocks - 1) && kc_file_type != BASIC_NO_HEADER && kc_file_type != BASIC_W_HEADER)
					buf[0]=0xff;
			}
			// Skip 0xff block for TAP_BASIC_EXTRA_BLOCKS
			if (kc_file_type == TAP_BASIC_EXTRA_BLOCKS && buf[0] == 0xff) {
				buf[0] = number_of_blocks;
			}
			
		}
		f_close(&fhdl);
	}
}
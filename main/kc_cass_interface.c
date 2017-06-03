/*
 * kc_cass_interface.c
 *
 * Created: 03.06.2017 14:29:04
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
#include "../ff_avr/xitoa.h"
#include "../ff_avr/ff.h"
#include "../ff_avr/diskio.h"

#include "kc_cass_interface.h"
#include "kc_cass_format_def.h"

#ifdef DEBUG_TIMER
#define TIMER_OVF_MONITOR_PORT PORTB
#define TIMER_OVF_MONITOR_DDR DDRB
#define TIMER_OVF_MONITOR_BIT PORTB6 //Pin 9 for ATMega328P
#endif

typedef enum {SPACE=0, ONE, ZERO} BIT_TYPE;

#define SEND_BUF_SIZE 130
uint8_t buf[SEND_BUF_SIZE]; //send buffer
uint8_t* start_buf_ptr = buf;

FIL fhdl;
FRESULT fr;

ISR(TIMER0_COMPA_vect)
{
	#ifdef DEBUG_TIMER
	TIMER_OVF_MONITOR_PORT |= _BV(TIMER_OVF_MONITOR_BIT);
	#endif
	switch(send_state)
	{
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
	TIMER_OVF_MONITOR_PORT &= ~_BV(TIMER_OVF_MONITOR_BIT);
	#endif
}

void timer0_init()
{
	TCCR0A = _BV(WGM01); // CTC mode -> TOP is OCR0A
	CASS_OUT_DDR |= _BV(CASS_OUT_PIN); // Configure output pin as OUTPUT
	CASS_OUT_PORT &= ~(_BV(CASS_OUT_PIN)); // Initialize output pin to LOW
	#ifdef DEBUG_TIMER
	TIMER_OVF_MONITOR_DDR |= _BV(TIMER_OVF_MONITOR_BIT);
	TIMER_OVF_MONITOR_PORT &= ~_BV(TIMER_OVF_MONITOR_BIT);
	#endif
}
// sends the specified bit using Timer0
// blocks until the bit is sent only if bit_type!=SPACE
// this gives us time to reload buffer and do computation while sending the space
static void send_bit(BIT_TYPE bit_type)
{
	send_state = FIRST_HALF;
	switch(bit_type)
	{
		case SPACE:
			OCR0A = F_CPU / 1024 / 571 / 2;
			break;
		case ONE:
			OCR0A = F_CPU / 1024 / 1087 / 2;
			break;
		case ZERO:
			OCR0A = F_CPU / 1024 / 2000 / 2;
			break;
	}
	TCNT0 = 0; // reset timer
	TIMSK0 = _BV(OCIE0A); // allow OCA interrupt generation
	CASS_OUT_PORT |= _BV(CASS_OUT_PIN);
	TCCR0B = 0b101; // start timer0 with 1024 prescaler >= 7812Hz for F_CPU=8MHz
	while (bit_type != SPACE && send_state != DONE)
	{
		// block until bit is sent
	}
}

// sends out a byte
// LSB first!
static void send_byte(uint8_t byte)
{
	while (send_state != DONE)
	{
		// wait until the previous SPACE is completed
	}
	uint8_t mask = 1;
	for (int i = 0; i < 8; i++)
	{
		if (mask & byte)
		{
			send_bit(ONE);
		}
		else
		{
			send_bit(ZERO);
		}
		mask <<= 1;
	}
	send_bit(SPACE); // this doesn't block!
}

UINT bytes_read = 0;
uint8_t block_len = 128;
bool has_checksum = false;
bool is_basic = false;

static bool load_first_block_and_check_type(FILINFO* Finfo)
{
	block_len = 129;
	start_buf_ptr = buf;
	if (Finfo->fsize < 128) //this doesnt make sense.
	return false;
	fr = f_open(&fhdl, Finfo->fname, FA_READ | FA_OPEN_EXISTING);
	if (fr != FR_OK)
	{
		f_close(&fhdl);
		return false;
	}
	// check for TAP header and guess existence of blocknumbers
	fr = f_read(&fhdl, buf, TAP_HEADER_LEN, &bytes_read);
	if (bytes_read != TAP_HEADER_LEN)
	{
		return false;
	}
	if (strncmp_P((char*)buf,tap_header_str,bytes_read))
	{
		// No TAP header
		if (Finfo->fsize % 129) // one block is 1 byte blocknr + 128byte data
		{
			block_len = 128;
			start_buf_ptr = buf + 1;
		}
		// No TAP header found in file -> rewind
		fr = f_lseek(&fhdl, 0);
		if (fr != FR_OK)
		{
			f_close(&fhdl);
			return false;
		}
		// load the first block completely
		fr = f_read(&fhdl, start_buf_ptr, block_len, &bytes_read);
		if (fr != FR_OK)
		{
			f_close(&fhdl);
			return false;
		}
		// check if BASIC header
		// check if MC header
		// check for extension SSS -> headerless BASIC files
		// none of the above so it is RAW
	}
	else
	{
		// it is definitely TAP format which always has blocknumbers
		// and doesnt need any other special recognition procedure
		// load the first block after the header
		fr = f_read(&fhdl, start_buf_ptr, block_len, &bytes_read);
		if (fr != FR_OK)
		{
			f_close(&fhdl);
			return false;
		}
	}
	// print file type on LCD
	return true;
}

void send_file(FILINFO* Finfo)
{
	if (load_first_block_and_check_type(Finfo))
	{
		do
		{
			// calculate checksum
			buf[129] = 0;
			for (int i = 1; i < 129; i++)
			{
				buf[129] += buf[i];
			}
			// Send "Vorton"
			switch (buf[0])
			{
				case 0:
					for (int j = 0; j < 6000; j++)
					send_bit(ONE);
					send_bit(SPACE);
					break;
				case 255:
					for (int j = 0; j < 5296; j++)
					send_bit(ONE);
					send_bit(SPACE);
					break;
				default:
					for (int j = 0; j < 160; j++)
					send_bit(ONE);
					send_bit(SPACE);
					break;
			}
			// send the block
			for (int i = 0; i<130; i++)
			{
				send_byte(buf[i]);
			}
			// read the next block
			fr = f_read(&fhdl, start_buf_ptr, block_len, &bytes_read);
			if (fr != FR_OK)
			{
				f_close(&fhdl);
				return;
			}
		}
		while (bytes_read == block_len);
		send_bit(SPACE); // Just to clean up
		f_close(&fhdl);
	}

}

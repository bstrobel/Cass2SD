/*
 * kc_cass_format_def.c
 *
 * Created: 06.06.2017 19:27:29
 *  Author: Bernd
 */ 
#define __PROG_TYPES_COMPAT__
#include <avr/pgmspace.h>
#include <avr/interrupt.h>
#include <avr/sfr_defs.h>
#include <stdbool.h>
#include "../ff_avr/ff.h"
#include "../ff_avr/diskio.h"
#include "debounced_keys.h"
#include "kc_cass_common.h"

DIR Dir;			/* http://elm-chan.org/fsw/ff/doc/sdir.html */
FILINFO Finfo; /* http://elm-chan.org/fsw/ff/doc/sfileinfo.html */
FRESULT fr;
FATFS FatFs;		/* File system object for each logical drive */
FIL fhdl;

char dir_name[DIR_NAME_SIZE]; // 8 char name, 1 char dot, 3 char ext, \0 byte
int16_t dir_idx = 0;

const char tap_header_str[] PROGMEM = "\xc3KC-TAPE by AF. ";

uint8_t buf[DATA_BUF_SIZE]; //send buffer

/*---------------------------------------------------------*/
/* disk_and_debounce_timer                                 */
/* 100Hz timer interrupt generated by Timer2               */
/*---------------------------------------------------------*/
volatile uint8_t timer_cntr = 0;
ISR(TIMER2_COMPA_vect)
{
	if (timer_cntr++ > 39) {
		disk_timerproc();	/* Drive timer procedure of low level disk I/O module */
		timer_cntr=0;
	}
	// check for button presses
	if ((keys_changed_bitmap & _BV(START_STOP_KEY)) && (~keys_bitmap & _BV(START_STOP_KEY)))
	{
		keys_changed_bitmap &= ~_BV(START_STOP_KEY);
		select_key_pressed = true;
	}
	// check for adjustment (rotary encoder)
	if (keys_changed_bitmap & _BV(ROTARY_A))
	{
		keys_changed_bitmap &= ~_BV(ROTARY_A);
		if (!(keys_bitmap & _BV(ROTARY_A))) // A changed to L
		{
			if (keys_bitmap & _BV(ROTARY_B)) // and B is H
			{
				display_task=UP;
			}
		}
	}
	if (keys_changed_bitmap & _BV(ROTARY_B))
	{
		keys_changed_bitmap &= ~_BV(ROTARY_B);
		if (!(keys_bitmap & _BV(ROTARY_B))) // B changed to L
		{
			if (keys_bitmap & _BV(ROTARY_A)) // and A is H
			{
				display_task=DOWN;
			}
		}
	}
	handle_keys();
}

void disk_and_debounce_timer_init (void)
{
	/* Start 100Hz system timer with TC0 */
	// F=8MHz, prescaler=256 => 1 tick = 32�s
	// TOP=255 => max timer tick = 8.192ms 
	TCCR2A = _BV(WGM21); // CTC mode
	OCR2A = 7; // 8 * 32�s = 0.256ms => 3.90625kHz
	disk_and_debounce_timer_start();
}

void disk_and_debounce_timer_start(void) {
	TCCR2B = _BV(CS22) | _BV(CS21); // 256 prescaler
	TIMSK2 = _BV(OCIE2A); // enable interrup at OC match A
}

void disk_and_debounce_timer_stop(void) {
	TIMSK2 = 0;
	TCCR2B = 0;
	TCNT2 = 0;
}

uint8_t calculate_checksum() {
	uint8_t chksum = 0;
	for (uint8_t i = 1; i < (DATA_BUF_SIZE - 1); i++) {
		chksum += buf[i];
	}
	return chksum;
}


bool check_is_basic_fcb() {
	KC_FCB_BASIC* fcb_basic = (KC_FCB_BASIC*) (buf + 1);
	uint8_t b1 = fcb_basic->dateityp[0];
	uint8_t b2 = fcb_basic->dateityp[1];
	uint8_t b3 = fcb_basic->dateityp[2];
	return ( b1 >= 0xd3 && b1 <= 0xd9 && b1 == b2 && b1 == b3);
}

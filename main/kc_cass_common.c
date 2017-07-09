/*
 * kc_cass_format_def.c
 *
 * Created: 06.06.2017 19:27:29
 *  Author: Bernd
 */ 
#define __PROG_TYPES_COMPAT__
#include <avr/pgmspace.h>
#include <stdbool.h>
#include "../ff_avr/ff.h"
#include "kc_cass_common.h"

DIR Dir;			/* http://elm-chan.org/fsw/ff/doc/sdir.html */
FILINFO Finfo; /* http://elm-chan.org/fsw/ff/doc/sfileinfo.html */
FRESULT fr;
FATFS FatFs;		/* File system object for each logical drive */
FIL fhdl;

const char tap_header_str[] PROGMEM = "\xc3KC-TAPE by AF. ";

uint8_t buf[DATA_BUF_SIZE]; //send buffer

uint8_t calculate_checksum() {
	uint8_t chksum = 0;
	for (uint8_t i = 1; i < 129; i++) {
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

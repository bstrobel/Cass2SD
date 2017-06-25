/*
 * kc_cass_format_def.c
 *
 * Created: 06.06.2017 19:27:29
 *  Author: Bernd
 */ 
#define __PROG_TYPES_COMPAT__
#include <avr/pgmspace.h>
#include "../ff_avr/ff.h"
#include "kc_cass_common.h"

DIR Dir;			/* http://elm-chan.org/fsw/ff/doc/sdir.html */
FILINFO Finfo; /* http://elm-chan.org/fsw/ff/doc/sfileinfo.html */
FRESULT fr;
FATFS FatFs;		/* File system object for each logical drive */
FIL fhdl;

const char tap_header_str[] PROGMEM = "\xc3KC-TAPE by AF. ";

uint8_t buf[DATA_BUF_SIZE]; //send buffer

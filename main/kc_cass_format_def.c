/*
 * kc_cass_format_def.c
 *
 * Created: 06.06.2017 19:27:29
 *  Author: Bernd
 */ 
#define __PROG_TYPES_COMPAT__
#include <avr/pgmspace.h>
#include "kc_cass_format_def.h"

const char tap_header_str[] PROGMEM = "\xc3KC-TAPE by AF. ";
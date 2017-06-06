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
#include <stdbool.h>
#include "../ff_avr/xitoa.h"
#include "../ff_avr/ff.h"
#include "../ff_avr/diskio.h"
#include "../lcd/lcd.h"
#include "kc_cass_recv_file.h"


ISR(INT0_vect) {

}


void kc_cass_recv_file_init() {
	CASS_IN_DDR &= ~_BV(CASS_IN_PIN); // Configure input pin as INPUT
	CASS_IN_PORT &= ~_BV(CASS_IN_PIN); // Disable pullup
	EIMSK &= ~_BV(INT0); // make sure INT0 pin interrupts are disabled
	EICRA = _BV(ISC00); // any change in logical level on pin will generate int
}


void recv_file(FILINFO* Finfo) {
	EIFR &= _BV(INT0); // clear int flag reg
	EIMSK |= _BV(INT0); // enable int INT0
}
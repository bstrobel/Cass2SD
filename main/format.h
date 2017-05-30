/*
 * format.h
 *
 * Created: 01.05.2017 13:32:31
 *  Author: Bernd
 */ 


#ifndef FORMAT_H_
#define FORMAT_H_

#include <stdint.h>
#include <avr/pgmspace.h>

const prog_char tap_header_str[] = "\xc3KC-TAPE by AF. ";
#define TAP_HEADER_LEN 16

typedef struct{
	char dateiname[8];
	char dateityp[3];
	uint8_t ext1;
	uint8_t ext2;
	uint8_t psum;
	uint8_t arb;
	uint8_t blnr;
	uint8_t lblnr;
	uint16_t aadr;
	uint16_t eadr;
	uint16_t sadr;
	uint8_t sby;
} KC_FCB_MC;

typedef struct 
{
	char dateityp[3];
	char dateiname[8];
	uint16_t adr1;
	uint16_t adr2;
} KC_FCB_BASIC;

#endif /* FORMAT_H_ */
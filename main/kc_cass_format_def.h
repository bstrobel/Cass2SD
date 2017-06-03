/*
 * kc_cass_format_def.h
 *
 * Created: 03.06.2017 14:51:38
 *  Author: Bernd
 */ 


#ifndef KC_CASS_FORMAT_DEF_H_
#define KC_CASS_FORMAT_DEF_H_

#include <stdint.h>
#define __PROG_TYPES_COMPAT__
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

#endif /* KC_CASS_FORMAT_DEF_H_ */
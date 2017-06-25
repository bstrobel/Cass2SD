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

#define DATA_BUF_SIZE 130
extern uint8_t buf[];

typedef enum {SPACE=0, ONE, ZERO} BIT_TYPE;

extern const char tap_header_str[] PROGMEM;
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
} KC_FCB_BASIC;
#define BASIC_HEADER_LEN 11

typedef enum
{
	BASIC_NO_HEADER,
	BASIC_W_HEADER,
	MACHINE_CODE,
	RAW,
	TAP,
	TAP_BASIC,
	TAP_BASIC_EXTRA_BLOCKS
} KC_FILE_TYPE;

#define VORTON_BEGIN 6000
#define VORTON_BLOCK 160
#define VORTON_FFBLOCK 5296

#endif /* KC_CASS_FORMAT_DEF_H_ */
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
#include "../ff_avr/ff.h"

extern DIR Dir;			/* http://elm-chan.org/fsw/ff/doc/sdir.html */
extern FILINFO Finfo; /* http://elm-chan.org/fsw/ff/doc/sfileinfo.html */
extern FRESULT fr;
extern FATFS FatFs;		/* File system object for each logical drive */
extern FIL fhdl;


#define DATA_BUF_SIZE 130
extern uint8_t buf[];

typedef enum {SPACE=0, ONE, ZERO} BIT_TYPE;

extern const char tap_header_str[] PROGMEM;
#define TAP_HEADER_LEN 16

#define LEN_DATEINAME 8
#define LEN_DATEITYP 3

typedef struct{
	char dateiname[LEN_DATEINAME];
	char dateityp[LEN_DATEITYP];
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
	char dateityp[LEN_DATEITYP];
	char dateiname[LEN_DATEINAME];
} KC_FCB_BASIC;
#define BASIC_HEADER_LEN (LEN_DATEINAME + LEN_DATEITYP)

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

uint8_t calculate_checksum(void);
bool check_is_basic_fcb(void);

#endif /* KC_CASS_FORMAT_DEF_H_ */
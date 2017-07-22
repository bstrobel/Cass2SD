/*
 * kc_cass_format_def.h
 *
 * Created: 03.06.2017 14:51:38
 *  Author: Bernd
 */ 


#ifndef KC_CASS_FORMAT_DEF_H_
#define KC_CASS_FORMAT_DEF_H_

#include <stdint.h>
#include <stdbool.h>
#define __PROG_TYPES_COMPAT__
#include <avr/pgmspace.h>
#include "../ff_avr/ff.h"

extern DIR Dir;			/* http://elm-chan.org/fsw/ff/doc/sdir.html */
extern FILINFO Finfo; /* http://elm-chan.org/fsw/ff/doc/sfileinfo.html */
extern FRESULT fr;
extern FATFS FatFs;		/* File system object for each logical drive */
extern FIL fhdl;

extern uint16_t disp_timer;
extern bool is_file_details_displayed;
#define DISP_FILE_DETAILS_TIMEOUT_COUNT 12000U


extern uint8_t* start_buf_ptr;
extern uint8_t block_len;
extern uint8_t number_of_blocks;

typedef enum
{
	BASIC_NO_HEADER,
	BASIC_W_HEADER,
	OTHER_THAN_BASIC,
	RAW,
	TAP,
	TAP_BASIC,
	TAP_BASIC_EXTRA_BLOCKS
} KC_FILE_TYPE;

extern KC_FILE_TYPE kc_file_type;

typedef enum {
	IDLE=0,
	SENDING,
	RECEIVING
} SYSTEM_STATE;

extern SYSTEM_STATE system_state;

#define DATA_BUF_SIZE 130
extern uint8_t buf[];

#define BLOCK_LEN_WITH_BLOCKNUM 129
#define BLOCK_LEN_WITHOUT_BLOCKNUM 128
#define HAS_NO_BLOCKNR (block_len == BLOCK_LEN_WITHOUT_BLOCKNUM)

#define TYPE_IS_BASIC ( \
kc_file_type == TAP_BASIC \
|| kc_file_type == TAP_BASIC_EXTRA_BLOCKS \
|| kc_file_type == BASIC_NO_HEADER \
|| kc_file_type == BASIC_W_HEADER \
)

#define TYPE_IS_BASIC_WITH_HEADER ( \
kc_file_type == TAP_BASIC \
|| kc_file_type == TAP_BASIC_EXTRA_BLOCKS \
|| kc_file_type == BASIC_W_HEADER \
)

#define TYPE_IS_WITH_HEADER_NO_BASIC ( \
kc_file_type == OTHER_THAN_BASIC \
|| kc_file_type == TAP \
)

#define TYPE_IS_NO_HEADER ( \
kc_file_type == RAW \
)

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
} KC_FCB;

typedef struct
{
	char dateityp[LEN_DATEITYP];
	char dateiname[LEN_DATEINAME];
} KC_FCB_BASIC;
#define BASIC_HEADER_LEN (LEN_DATEINAME + LEN_DATEITYP)

#define VORTON_BEGIN 6000
#define VORTON_BLOCK 160
#define VORTON_FFBLOCK 5296

uint8_t calculate_checksum(void);
bool check_is_basic_fcb(void);
void disk_and_debounce_timer_init (void);
void disk_and_debounce_timer_start(void);
void disk_and_debounce_timer_stop(void);
bool load_first_block_and_check_type(FILINFO* Finfo);
#endif /* KC_CASS_FORMAT_DEF_H_ */
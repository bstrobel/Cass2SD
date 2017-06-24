/*
 * display_util.c
 *
 * Created: 03.06.2017 15:59:28
 *  Author: Bernd
 */ 
#define __PROG_TYPES_COMPAT__
#include <avr/pgmspace.h>
#include <util/delay.h>
#include <stdbool.h>
#include "../ff_avr/ff.h"
#include "../ff_avr/xitoa.h"
#include "../lcd/lcd.h"
#include "display_util.h"
#include "debounced_keys.h"

const char dotdotdir_str[] PROGMEM = ".. [GO UP]";
const char record_cmd_str[] PROGMEM = "[RECORD HERE]";
const char dir_str[] PROGMEM = "[DIR]";
const char empty_dir_str[] PROGMEM = "[Empty Dir]";
const char pct_s_str[] PROGMEM = "%s";
const char pct_d_str[] PROGMEM = "%d";
const char pct_X_str[] PROGMEM = "0x%02X 0x%02X 0x%02X";

char dir_name[DIR_NAME_SIZE]; // 8 char name, 1 char dot, 3 char ext, \0 byte
int16_t dir_idx = 0;

bool disp_fr_err(FRESULT fr) {
	if (fr != FR_OK) {
		display_fresult(fr);
		_delay_ms(ERROR_DISP_MILLIS);
		return true;
	}
	else
	return false;
}

void display_fresult (FRESULT rc)
{
	PGM_P p;
	static const char str[] PROGMEM =
	"OK\0DISK_ERR\0INT_ERR\0NOT_READY\0NO_FILE\0NO_PATH\0INVALID_NAME\0"
	"DENIED\0EXIST\0INVALID_OBJECT\0WRITE_PROTECTED\0INVALID_DRIVE\0"
	"NOT_ENABLED\0NO_FILE_SYSTEM\0MKFS_ABORTED\0TIMEOUT\0LOCKED\0"
	"NOT_ENOUGH_CORE\0TOO_MANY_OPEN_FILES\0";
	FRESULT i;

	for (p = str, i = 0; i != rc && pgm_read_byte_near(p); i++) {
		while(pgm_read_byte_near(p++));
	}
	lcd_clrscr();
	xprintf(PSTR("rc=%u"), rc);
	lcd_gotoxy(0,1);
	xprintf(PSTR("FR_%S"), p);
}

void display_fileinfo(FILINFO* Finfo)
{
	lcd_clrscr();
	switch (dir_idx)
	{
		case DIR_IDX_REC:
		{
			xprintf(pct_s_str, dir_name);
			lcd_gotoxy(0,1);
			xprintf(record_cmd_str);
			break;
		}
		case DIR_IDX_GO_UP:
		{
			xprintf(pct_s_str, dir_name);
			lcd_gotoxy(0,1);
			xprintf(dotdotdir_str);
			break;
		}
		default:
		{
			if (dir_idx == DIR_IDX_FIRST_FILE && !Finfo->fname[0])
			{
				xprintf(empty_dir_str);
			}
			else
			{
				xprintf(pct_s_str, Finfo->fname);
				lcd_gotoxy(0,1);
				if (Finfo->fattrib & AM_DIR) {
					xprintf(dir_str);
				}
				else
				{
					xprintf(pct_d_str, Finfo->fsize);
				}
			}
			break;
		}
	}
}

void display_sendinfo(char* filename, uint8_t block_len, uint8_t num_blocks, KC_FILE_TYPE file_type)
{
	lcd_clrscr();
	xprintf(PSTR("SND:%s"),filename);
	lcd_gotoxy(0,1);
	char* t;
	switch(file_type)
	{
		case BASIC_NO_HEADER:
			t="B_NOHD";
			break;
		case BASIC_W_HEADER:
			t="B_WHDR";
			break;
		case MACHINE_CODE:
			t="MC";
			break;
		case TAP:
			t="TAP_MC";
			break;
		case TAP_BASIC:
			t="TAP_B";
			break;
		case TAP_BASIC_EXTRA_BLOCKS:
			t="TAP_BX";
			break;
		default:
		case RAW:
			t="RAW";
			break;
	}
	xprintf(PSTR("#XXX/%03d%c %s"),num_blocks,block_len==128?'!':' ', t);
}

void display_upd_sendinfo(uint8_t blocknr)
{
	lcd_gotoxy(1,1);
	xprintf(PSTR("%03d"),blocknr);
}
/* Format string is placed in the ROM. The format flags is similar to printf().

   %[flag][width][size]type

   flag
     A '0' means filled with '0' when output is shorter than width.
     ' ' is used in default. This is effective only numeral type.
   width
     Minimum width in decimal number. This is effective only numeral type.
     Default width is zero.
   size
     A 'l' means the argument is long(32bit). Default is short(16bit).
     This is effective only numeral type.
   type
     'c' : Character, argument is the value
     's' : String placed on the RAM, argument is the pointer
     'S' : String placed on the ROM, argument is the pointer
     'd' : Signed decimal, argument is the value
     'u' : Unsigned decimal, argument is the value
     'X' : Hexdecimal, argument is the value
     'b' : Binary, argument is the value
     '%' : '%'

*/

void disp_err(char* line1, char* line2) {
	lcd_clrscr();
	xprintf(pct_s_str,line1);
	lcd_gotoxy(0,1);
	xprintf(pct_s_str,line2);
	_delay_ms(ERROR_DISP_MILLIS);
}

#ifdef DEBUG
void display_debug_and_block(char* line1, uint8_t val1, uint8_t val2, uint8_t val3)
{
	lcd_clrscr();
	xprintf(pct_s_str,line1);
	lcd_gotoxy(0,1);
	xprintf(pct_X_str, val1, val2, val3);
	select_key_pressed = 0;
	while(!select_key_pressed);
}
#endif
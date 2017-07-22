/*
 * display_util.c
 *
 * Created: 03.06.2017 15:59:28
 *  Author: Bernd
 */ 
#define __PROG_TYPES_COMPAT__
#include <avr/pgmspace.h>
#include <util/delay.h>
#include <stddef.h>
#include <stdbool.h>
#include <stdarg.h>
#include <string.h>
#include "../ff_avr/ff.h"
#include "../ff_avr/xitoa.h"
#include "../lcd/lcd.h"
#include "kc_cass_common.h"
#include "debounced_keys.h"
#include "display_util.h"

const char dotdotdir_str[] PROGMEM = ".. [GO UP]";
const char dir_str[] PROGMEM = "[DIR]";
const char empty_dir_str[] PROGMEM = "[Empty Dir]";
const char pct_s_str[] PROGMEM = "%s";
const char pct_u_str[] PROGMEM = "%lu";
const char pct_X_str[] PROGMEM = "0x%02X 0x%02X 0x%02X";
const char msg_error_str[] PROGMEM = "ERROR";
const char msg_info_str[] PROGMEM = "INFO";
const char msg_block_too_short_str[] PROGMEM = "BLOCK TOO SHORT!";
const char vol_name_str[] PROGMEM = "VOL:%s";
const char vol_free_str_unknown[] PROGMEM = "FR:%luMB";
const char vol_free_str_FS_FAT12[] PROGMEM = "FAT12 FR:%luMB";
const char vol_free_str_FS_FAT16[] PROGMEM = "FAT16 FR:%luMB";
const char vol_free_str_FS_FAT32[] PROGMEM = "FAT32 FR:%luMB";
const char vol_free_str_FS_EXFAT[] PROGMEM = "EXFAT FR:%luMB";
const char file_type_str_B_NOHD[] PROGMEM ="B_NOHD";
const char file_type_str_B_WHDR[] PROGMEM ="B_WHDR";
const char file_type_str_OTHER[] PROGMEM ="OTHER";
const char file_type_str_TAP_MC[] PROGMEM ="TAP";
const char file_type_str_TAP_B[] PROGMEM ="TAP_B";
const char file_type_str_TAP_BX[] PROGMEM ="TAP_BX";
const char file_type_str_RAW[] PROGMEM ="RAW";

#define MAX_PATH_LENGTH 64
char dir_name[DIR_NAME_SIZE]; // 8 char name, 1 char dot, 3 char ext, \0 byte
int16_t dir_idx = 0;

void display_prev()
{
	if (dir_idx > 0)
	{
		dir_idx--;
		if (dir_idx == 0) {
			f_readdir(&Dir,NULL); // rewinds to first item
		}
		if (dir_idx >= DIR_IDX_FIRST_FILE)
		{
			// FAT lib doesn't provide a way to go backwards through the list of files
			// So we close the dir and reopen it and flip through until the new dir_idx
			f_readdir(&Dir,NULL); // rewinds to first item
			for (int16_t i=DIR_IDX_FIRST_FILE; i<=dir_idx; i++)
			{
				f_readdir(&Dir,&Finfo);
			}
		}
		display_fileinfo(&Finfo);
	}
}

void display_next()
{
	if (dir_idx < INT16_MAX)
	{
		dir_idx++;
		if (dir_idx == DIR_IDX_GO_UP) {
			f_readdir(&Dir,NULL); // "rewind" to the start of Dir 
		}
		else if (dir_idx >= DIR_IDX_FIRST_FILE) {
			f_readdir(&Dir,&Finfo);
			if (!Finfo.fname[0]) {
				display_prev(); // We're over the edge, step back!
				return;
			}
		}
	}
	display_fileinfo(&Finfo);
}

void display_by_name(char* name, bool is_dir) {
	f_readdir(&Dir,NULL); // rewinds to first item
	for (dir_idx = DIR_IDX_FIRST_FILE;dir_idx <= INT16_MAX; dir_idx++) {
		f_readdir(&Dir,&Finfo);
		if (!Finfo.fname[0])
		{
			dir_idx=-1;
			display_next(); // not found, show beginning
			return;			
		}
		if (!strcmp(Finfo.fname,name)) {
			if ((Finfo.fattrib & AM_DIR) && is_dir) {
				display_fileinfo(&Finfo);
				return;
			}
			if (!(Finfo.fattrib & AM_DIR) && !is_dir) {
				display_fileinfo(&Finfo);
				return;
			}
		}
	}
}

bool disp_fr_err(FRESULT r) {
	if (r != FR_OK) {
		display_fresult(r);
		_delay_ms(ERROR_DISP_MILLIS);
		return true;
	}
	else {
		return false;
	}
}

void display_fresult (FRESULT rc)
{
	fr = rc;
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
	is_file_details_displayed = false;
	disp_timer = 0;
	lcd_clrscr();
	switch (dir_idx)
	{
		case DIR_IDX_GO_UP:
		{
			if (dir_name[0]) { // sub dir
				xprintf(pct_s_str, dir_name);
				lcd_gotoxy(0,1);
				xprintf(dotdotdir_str);
			}
			else { // root dir
				char label[24];
				f_getlabel("",label,NULL);
				xprintf(vol_name_str, label);
				lcd_gotoxy(0,1);
				FATFS* fs;
				DWORD free_clusters;
				f_getfree("",&free_clusters,&fs);
				#if _MAX_SS != _MIN_SS
				DWORD free_mbytes = free_clusters * FatFs.csize * FatFs.ssize / (1024UL * 1024UL);
				#else
				DWORD free_mbytes = free_clusters * FatFs.csize * _MAX_SS / (1024UL * 1024UL);
				#endif
				switch (FatFs.fs_type) {
					case FS_EXFAT: {
						xprintf(vol_free_str_FS_EXFAT,free_mbytes);
						break;
					}
					case FS_FAT12: {
						xprintf(vol_free_str_FS_FAT12,free_mbytes);
						break;
					}
					case FS_FAT16: {
						xprintf(vol_free_str_FS_FAT16,free_mbytes);
						break;
					}
					case FS_FAT32: {
						xprintf(vol_free_str_FS_FAT32,free_mbytes);
						break;
					}
					default: {
						xprintf(vol_free_str_unknown,free_mbytes);
						break;
					}
				}
			}
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
					xprintf(pct_u_str, Finfo->fsize);
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
	PGM_P t;
	switch(file_type)
	{
		case BASIC_NO_HEADER: {
			t=file_type_str_B_NOHD;
			break;
		}
		case BASIC_W_HEADER: {
			t=file_type_str_B_WHDR;
			break;
		}
		case OTHER_THAN_BASIC: {
			t=file_type_str_OTHER;
			break;
		}
		case TAP: {
			t=file_type_str_TAP_MC;
			break;
		}
		case TAP_BASIC: {
			t=file_type_str_TAP_B;
			break;
		}
		case TAP_BASIC_EXTRA_BLOCKS: {
			t=file_type_str_TAP_BX;
			break;
		}
		default:
		case RAW: {
			t=file_type_str_RAW;
			break;
		}
	}
	xprintf(PSTR("#XXX/%03d%c %S"),num_blocks,block_len==128?'!':' ', t);
}

void display_upd_sendinfo(uint8_t blocknr)
{
	lcd_gotoxy(1,1);
	xprintf(PSTR("%03d"),blocknr);
}

void display_recvinfo(char* filename, uint8_t blocknr, char* filetype) {
	lcd_clrscr();
	if (filename == NULL) {
		xprintf(PSTR("RCV:????????.???"));
		lcd_gotoxy(0,1);
		xprintf(PSTR("#??? TYPE:???"));
	}
	else {
		xprintf(PSTR("RCV:%s"),filename);
		lcd_gotoxy(0,1);
		xprintf(PSTR("#%03d TYPE:%s"),blocknr,filetype);
	}
}

void display_upd_recvinfo(uint8_t blocknr)
{
	lcd_gotoxy(1,1);
	xprintf(PSTR("%03d"),blocknr);
}

void disp_msg_p(const char* PROGMEM line1, const char* PROGMEM line2) {
	lcd_clrscr();
	xprintf(line1);
	lcd_gotoxy(0,1);
	xprintf(line2);
	_delay_ms(ERROR_DISP_MILLIS);
}

void disp_util_fill_dir_name() {
	char cwd[MAX_PATH_LENGTH];
	f_getcwd(cwd, MAX_PATH_LENGTH);
	uint8_t str_end = 0;
	for (str_end=0;str_end < DIR_NAME_SIZE && cwd[str_end]; str_end++);
	for (uint8_t i = str_end - 1; i>=0; i--) {
		if (cwd[i] == '/') {
			strlcpy(dir_name,cwd+i+1,DIR_NAME_SIZE);
			break;
		}
	}
	
}

void display_file_details() {
	if (dir_idx >= DIR_IDX_FIRST_FILE && !(Finfo.fattrib & AM_DIR) && load_first_block_and_check_type(&Finfo)) {
		lcd_clrscr();
		#define LEN_FILENAME 9
		#define LEN_FILETYPE 4
		char filename[LEN_FILENAME];
		char fileext[LEN_FILETYPE];
		if (TYPE_IS_BASIC_WITH_HEADER) {
			KC_FCB_BASIC* fcb = (KC_FCB_BASIC*) (buf + 1);
			strlcpy(filename,fcb->dateiname,LEN_FILENAME);
			strlcpy(fileext,fcb->dateityp,LEN_FILETYPE);
			for (uint8_t i = 0; i < LEN_FILETYPE - 1; i++) {
				fileext[i] -= 0x80;
			}
		}
		else if (TYPE_IS_WITH_HEADER_NO_BASIC) {
			KC_FCB* fcb = (KC_FCB*) (buf + 1);
			strlcpy(filename,fcb->dateiname,LEN_FILENAME);
			strlcpy(fileext,fcb->dateityp,LEN_FILETYPE);
		}
		else { // no header -> get info from filename
			char* ext = strchr(Finfo.fname,'.');
			char* src = Finfo.fname;
			char* dst = filename;
			do {
				*dst = *src;
				src++;
				dst++;
			} 
			while (src < ext && dst < filename + LEN_FILENAME);
			*dst = 0;
			src = ext + 1;
			dst = fileext;
			do {
				*dst = *src;
				src++;
				dst++;
			}
			while (*src && src < ext+4 && dst < fileext + LEN_FILETYPE);
			*dst = 0;
		}
		xprintf(PSTR("%s [%s]"),filename, fileext);
		lcd_gotoxy(0,1);
		PGM_P t;
		switch(kc_file_type)
		{
			case BASIC_NO_HEADER: {
				t=file_type_str_B_NOHD;
				break;
			}
			case BASIC_W_HEADER: {
				t=file_type_str_B_WHDR;
				break;
			}
			case OTHER_THAN_BASIC: {
				t=file_type_str_OTHER;
				break;
			}
			case TAP: {
				t=file_type_str_TAP_MC;
				break;
			}
			case TAP_BASIC: {
				t=file_type_str_TAP_B;
				break;
			}
			case TAP_BASIC_EXTRA_BLOCKS: {
				t=file_type_str_TAP_BX;
				break;
			}
			default:
			case RAW: {
				t=file_type_str_RAW;
				break;
			}
		}
		xprintf(PSTR("#%03d%c %S"),number_of_blocks,block_len==128?'!':' ', t);
	}
	is_file_details_displayed = true;
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
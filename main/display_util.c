/*
 * display_util.c
 *
 * Created: 03.06.2017 15:59:28
 *  Author: Bernd
 */ 
#define __PROG_TYPES_COMPAT__
#include <avr/pgmspace.h>
#include "../ff_avr/ff.h"
#include "../ff_avr/xitoa.h"
#include "../lcd/lcd.h"
#include "display_util.h"

const prog_char dotdotdir_str[] = ".. [GO UP]";
const prog_char record_cmd_str[] = "[RECORD HERE]";
const prog_char dir_str[] = "[DIR]";
const prog_char empty_dir_str[] = "[Empty Dir]";
const prog_char pct_s_str[] = "%s";
const prog_char pct_d_str[] = "%d";

char dir_name[DIR_NAME_SIZE]; // 8 char name, 1 char dot, 3 char ext, \0 byte
int16_t dir_idx = 0;

void put_rc (FRESULT rc)
{
	const prog_char *p;
	static const prog_char str[] =
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

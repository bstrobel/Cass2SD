/*
* main_ff_avr.c
*
* Created: 29.04.2017 12:44:25
*  Author: Bernd
*/
#include <avr/io.h>
#include <avr/interrupt.h>
#include <avr/sfr_defs.h>
#include <avr/sleep.h>
#include <avr/power.h>
#define __PROG_TYPES_COMPAT__
#include <avr/pgmspace.h>
#include <string.h>
#include <util/delay.h>
#include <stdbool.h>
#include "../ff_avr/xitoa.h"
#include "../ff_avr/ff.h"
#include "../ff_avr/diskio.h"
#include "../lcd/lcd.h"
#include "debounced_keys.h"
#include "kc_cass_interface.h"

#define START_STOP_KEY 0
#define ROTARY_A START_STOP_KEY + 1
#define ROTARY_B ROTARY_A + 1

typedef enum {STAY=0, UP, DOWN} DIRECTION;

volatile DIRECTION display_task = STAY;
volatile bool select_key_pressed;

const prog_char dotdotdir_str[] = ".. [GO UP]";
const prog_char record_cmd_str[] = "[RECORD HERE]";
const prog_char dir_str[] = "[DIR]";
const prog_char empty_dir_str[] = "[Empty Dir]";
const prog_char pct_s_str[] = "%s";
const prog_char pct_d_str[] = "%d";

#define DIR_IDX_REC 0
#define DIR_IDX_GO_UP DIR_IDX_REC + 1
#define DIR_IDX_FIRST_FILE DIR_IDX_GO_UP + 1

DWORD sect = 0;
DSTATUS init_stat = STA_NOINIT;
DIR Dir;			/* http://elm-chan.org/fsw/ff/doc/sdir.html */
#define DIR_NAME_SIZE 13
char dir_name[DIR_NAME_SIZE]; // 8 char name, 1 char dot, 3 char ext, \0 byte
FILINFO Finfo; /* http://elm-chan.org/fsw/ff/doc/sfileinfo.html */
FIL fhdl;
FRESULT fr;
FATFS FatFs;		/* File system object for each logical drive */
int16_t dir_idx = 0;

static void put_rc (FRESULT rc)
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

/*---------------------------------------------------------*/
/* 100Hz timer interrupt generated by Timer2               */
/* Wakes AVR from Power Sleep mode.                        */
/*---------------------------------------------------------*/
ISR(TIMER2_COMPA_vect)
{
	disk_timerproc();	/* Drive timer procedure of low level disk I/O module */
	// check for button presses
	if ((keys_changed_bitmap & _BV(START_STOP_KEY)) && (~keys_bitmap & _BV(START_STOP_KEY)))
	{
		keys_changed_bitmap &= ~_BV(START_STOP_KEY);
		select_key_pressed = true;
	}
	// check for adjustment (rotary encoder)
	if (keys_changed_bitmap & _BV(ROTARY_A))
	{
		keys_changed_bitmap &= ~_BV(ROTARY_A);
		if (!(keys_bitmap & _BV(ROTARY_A))) // A changed to L
		{
			if (keys_bitmap & _BV(ROTARY_B)) // and B is H
			{
				display_task=UP;
			}
		}
	}
	if (keys_changed_bitmap & _BV(ROTARY_B))
	{
		keys_changed_bitmap &= ~_BV(ROTARY_B);
		if (!(keys_bitmap & _BV(ROTARY_B))) // B changed to L
		{
			if (keys_bitmap & _BV(ROTARY_A)) // and A is H
			{
				display_task=DOWN;
			}
		}
	}
	handle_keys();
}

static
void timer2_init (void)
{
	/* Start 100Hz system timer with TC0 */
	OCR2A = F_CPU / 1024 / 100 - 1; // OC reg A
	TCCR2A = _BV(WGM01); // CTC mode -> TOP is OCR2A
	TCCR2B = 0b101; // 1024 prescaler
	TIMSK2 = _BV(OCIE2A); // int at OC match A
}


static void display_fileinfo()
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
			if (dir_idx == DIR_IDX_FIRST_FILE && !Finfo.fname[0])
			{
				xprintf(empty_dir_str);
			}
			else
			{
				xprintf(pct_s_str, Finfo.fname);
				lcd_gotoxy(0,1);
				if (Finfo.fattrib & AM_DIR) {
					xprintf(dir_str);
				}
				else
				{
					xprintf(pct_d_str, Finfo.fsize);
			}
			}
			break;
		}
	}
}

static void display_prev()
{
	if (dir_idx > 0)
	{
		dir_idx--;
		f_closedir(&Dir);
		if (dir_idx >= DIR_IDX_FIRST_FILE)
		{
			// FAT lib doesn't provide a way to go backwards through the list of files
			// So we close the dir and reopen it and flip through until the new dir_idx
			f_opendir(&Dir,".");
			for (int16_t i=DIR_IDX_FIRST_FILE; i<=dir_idx; i++)
			{
				f_readdir(&Dir,&Finfo);
			}
		}
		display_fileinfo();
	}

}

static void display_next()
{
	if (dir_idx < INT16_MAX)
	{
		dir_idx++;
		if (dir_idx == DIR_IDX_FIRST_FILE)
		{
			f_opendir(&Dir,".");
		}
		if (dir_idx >= DIR_IDX_FIRST_FILE)
		{
			f_readdir(&Dir,&Finfo);
			if (!Finfo.fname[0])
			{
				display_prev(); // We're over the edge, step back!
				return;
			}
		}
	}
	display_fileinfo();
}

/************************************************************************/
/* To be called at the very beginning                                   */
/************************************************************************/
static void power_save_config()
{
	// ports are configured as input by default.
	// this enables the pullups.
	PORTB = 0xFF;
	PORTC = 0xFF;
	PORTD = 0xFF;
	power_twi_disable();
	power_adc_disable();
	power_timer1_disable();
	power_usart0_disable();
	set_sleep_mode(SLEEP_MODE_IDLE);
	//set_sleep_mode(SLEEP_MODE_PWR_SAVE);
	sleep_bod_disable();
	sleep_enable();
}

int main(void)
{
	power_save_config();
	timer0_init();
	timer2_init();				/* Initialize port settings and start system timer process */
	lcd_init(LCD_DISP_ON);
	xdev_out(lcd_putc);
	keys_init();
	sei();
	do 
	{
		fr = f_mount(&FatFs, "", 1);
		if (fr != FR_OK)
		{
			put_rc(fr);
			_delay_ms(500);
		}
	} 
	while (fr != FR_OK);
	
	f_getcwd(dir_name,DIR_NAME_SIZE);
	dir_idx = -1;
	display_next();
	
	while(1)
	{
		switch(display_task)
		{
		case UP:
			display_prev();
			display_task=STAY;
			break;
		case DOWN:
			display_next();
			display_task=STAY;
			break;
		case STAY:
			break;
		}
		if (select_key_pressed)
		{
			switch (dir_idx)
			{
				case DIR_IDX_REC:
				{
					break;
				}
				case DIR_IDX_GO_UP:
				{
					f_chdir("..");
					f_getcwd(dir_name, DIR_NAME_SIZE);
					dir_idx = -1;
					display_next();
					break;
				}
				default:
				{
					if (Finfo.fattrib & AM_DIR)
					{
						f_chdir(Finfo.fname);
						f_getcwd(dir_name, DIR_NAME_SIZE);
						dir_idx = -1;
						display_next();
					}
					else
					{
						if (!Finfo.fname[0])
						{
							send_file(&Finfo);
							while(send_state!=DONE)
							{
								// wait for finish before going to sleep
							}
						}
					}
					break;
				}
			}
			select_key_pressed = false;
		}
		sleep_cpu();
	}
	
}
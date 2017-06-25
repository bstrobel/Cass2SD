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
#include "display_util.h"
#include "kc_cass_send_file.h"
#include "kc_cass_recv_file.h"
#include "kc_cass_common.h"

/*---------------------------------------------------------*/
/* disk_and_debounce_timer                                 */
/* 100Hz timer interrupt generated by Timer2               */
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

static void disk_and_debounce_timer_init (void)
{
	/* Start 100Hz system timer with TC0 */
	OCR2A = F_CPU / 1024 / 100 - 1; // OC reg A
	TCCR2A = _BV(WGM01); // CTC mode -> TOP is OCR2A
	TCCR2B = 0b101; // 1024 prescaler
	TIMSK2 = _BV(OCIE2A); // int at OC match A
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
		display_fileinfo(&Finfo);
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
	display_fileinfo(&Finfo);
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
	// do the init stuff
	power_save_config();
	lcd_init(LCD_DISP_ON);
	xdev_out(lcd_putc);
	keys_init();
	kc_cass_send_file_init();
	kc_cass_recv_file_init();
	disk_and_debounce_timer_init();
	sei();

	// mount the SD card
	do 
	{
		fr = f_mount(&FatFs, "", 1);
		if (fr != FR_OK)
		{
			display_fresult(fr);
			_delay_ms(500);
		}
	} 
	while (fr != FR_OK);
	
	// initialize the menu system and display greeting
	f_getcwd(dir_name,DIR_NAME_SIZE);
	dir_idx = -1;
	display_next();
	
	// main loop
	while(1)
	{
		kc_cass_handle_recv_file();
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
						if (Finfo.fname[0])
						{
							send_file(&Finfo);
							while(send_state!=DONE)
							{
								// wait for finish before going to sleep
							}
							display_fileinfo(&Finfo);
						}
					}
					break;
				}
			}
			select_key_pressed = false;
		}
		// sleep until an interrupt wakes us up
		// (disk_and_debounce_timer does this 100 times a second)
		sleep_cpu();
	}
	
}
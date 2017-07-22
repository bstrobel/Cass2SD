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
	power_usart0_disable();
	set_sleep_mode(SLEEP_MODE_IDLE);
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
	sei();
	disk_and_debounce_timer_init();

	lcd_clrscr();
	xprintf(PSTR("INITIALIZING"));
	lcd_gotoxy(0,1);
	xprintf(PSTR("SD CARD"));
	// mount the SD card
	{
		char chars[4] = {'-','+','|','#'};
		uint8_t i = 0;
		do
		{
			fr = f_mount(&FatFs, "", 1);
			if (fr != FR_OK)
			{
				if (fr == FR_NOT_READY) {
					lcd_gotoxy(15,1);
					lcd_putc(chars[i++]);
					if (i > 3) {
						i=0;
					}
				}
				else {
					disp_fr_err(fr);
				
				}
			}
		}
		while (fr != FR_OK);
	}
	
	kc_cass_send_file_init();
	kc_cass_recv_file_init();
	
	// initialize the menu system and display greeting
	f_opendir(&Dir,"/");
	disp_util_fill_dir_name();
	dir_idx = -1;
	display_next();
	
	// main loop
	while(1)
	{
		kc_cass_handle_recv_file();
		if (system_state != IDLE) {
			continue; // we dont want to sleep while we receive or send a file
		}
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
					disp_util_fill_dir_name();
					f_chdir("..");
					f_opendir(&Dir,".");
					display_by_name(dir_name, true);
					disp_util_fill_dir_name();
					break;
				}
				default:
				{
					if (Finfo.fattrib & AM_DIR)
					{
						f_chdir(Finfo.fname);
						disp_util_fill_dir_name();
						f_opendir(&Dir,".");
						dir_idx = -1;
						display_next();
					}
					else
					{
						if (Finfo.fname[0])
						{
							send_file(&Finfo);
						}
					}
					break;
				}
			}
			is_file_details_displayed = false;
			select_key_pressed = false;
		}
		disp_timer++;
		if (disp_timer > DISP_FILE_DETAILS_TIMEOUT_COUNT && !is_file_details_displayed && system_state == IDLE) {
			display_file_details();
		}
		// sleep until an interrupt wakes us up
		// (disk_and_debounce_timer does this ~4000 times a second)
		sleep_cpu();
	}
	
}
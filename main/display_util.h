/*
 * display_util.h
 *
 * Created: 03.06.2017 15:59:13
 *  Author: Bernd
 */ 


#ifndef DISPLAY_UTIL_H_
#define DISPLAY_UTIL_H_
#include <stdbool.h>
#include <avr/pgmspace.h>
#include "../ff_avr/ff.h"
#include "kc_cass_common.h"

#define ERROR_DISP_MILLIS 5000

extern const char msg_error_str[] PROGMEM;
extern const char msg_info_str[] PROGMEM;

void display_prev(void);
void display_next(void);
void display_fileinfo(FILINFO*);
void display_sendinfo(char* filename, uint8_t block_len, uint8_t num_blocks, KC_FILE_TYPE file_type);
void display_upd_sendinfo(uint8_t blocknr);
void display_recvinfo(char* filename, uint8_t blocknr, char* filetype);
void display_upd_recvinfo(uint8_t blocknr);
void display_fresult (FRESULT);
void disp_msg_p(const char* PROGMEM line1, const char* PROGMEM line2);
bool disp_fr_err(FRESULT);
#ifdef DEBUG
void display_debug_and_block(char* line1, uint8_t val1, uint8_t val2, uint8_t val3);
#endif

#endif /* DISPLAY_UTIL_H_ */
/*
 * display_util.h
 *
 * Created: 03.06.2017 15:59:13
 *  Author: Bernd
 */ 


#ifndef DISPLAY_UTIL_H_
#define DISPLAY_UTIL_H_
#include <stdbool.h>
#include "../ff_avr/ff.h"
#include "kc_cass_format_def.h"

#define DIR_IDX_REC 0
#define DIR_IDX_GO_UP DIR_IDX_REC + 1
#define DIR_IDX_FIRST_FILE DIR_IDX_GO_UP + 1
#define DIR_NAME_SIZE 13

#define ERROR_DISP_MILLIS 5000

extern char dir_name[]; // 8 char name, 1 char dot, 3 char ext, \0 byte
extern int16_t dir_idx;

void display_fileinfo(FILINFO*);
void display_sendinfo(char* filename, uint8_t block_len, uint8_t num_blocks, KC_FILE_TYPE file_type);
void display_upd_sendinfo(uint8_t blocknr);
void put_rc (FRESULT);
void disp_err(char* line1, char* line2);
bool disp_fr_err(FRESULT);
#ifdef DEBUG
void display_debug_and_block(char* line1, uint8_t val1, uint8_t val2, uint8_t val3);
#endif

#endif /* DISPLAY_UTIL_H_ */
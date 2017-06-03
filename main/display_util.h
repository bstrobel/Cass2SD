/*
 * display_util.h
 *
 * Created: 03.06.2017 15:59:13
 *  Author: Bernd
 */ 


#ifndef DISPLAY_UTIL_H_
#define DISPLAY_UTIL_H_
#include "../ff_avr/ff.h"

#define DIR_IDX_REC 0
#define DIR_IDX_GO_UP DIR_IDX_REC + 1
#define DIR_IDX_FIRST_FILE DIR_IDX_GO_UP + 1
#define DIR_NAME_SIZE 13

extern char dir_name[]; // 8 char name, 1 char dot, 3 char ext, \0 byte
extern int16_t dir_idx;


void display_fileinfo(FILINFO*);
void put_rc (FRESULT);

#endif /* DISPLAY_UTIL_H_ */
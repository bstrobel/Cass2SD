/*
 * kc_cass_interface.h
 *
 * Created: 03.06.2017 14:28:44
 *  Author: Bernd
 */ 


#ifndef KC_CASS_SEND_FILE_H_
#define KC_CASS_SEND_FILE_H_

#define CASS_OUT_PORT PORTD
#define CASS_OUT_DDR DDRD
#define CASS_OUT_PIN PORTD1 //Pin 3 for ATMega328P

#ifdef DEBUG_TIMER
#define MONITOR_PORT PORTB
#define MONITOR_DDR DDRB
#define MONITOR_PIN PINB
#define MONITOR_BIT PORTB6 //Pin 9 for ATMega328P
#endif

typedef enum {DONE=0, FIRST_HALF, SECOND_HALF} SEND_STATE;
extern volatile SEND_STATE send_state;

void send_file(FILINFO*);
void kc_cass_send_file_init(void);

#endif /* KC_CASS_SEND_FILE_H_ */
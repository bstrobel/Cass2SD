/*
 * kc_cass_interface.h
 *
 * Created: 03.06.2017 14:28:44
 *  Author: Bernd
 */ 


#ifndef KC_CASS_INTERFACE_H_
#define KC_CASS_INTERFACE_H_

#define CASS_OUT_PORT PORTD
#define CASS_OUT_DDR DDRD
#define CASS_OUT_PIN PORTD1 //Pin 3 for ATMega328P
#define CASS_IN_PORT PORTD
#define CASS_IN_DDR DDRD
#define CASS_IN_PIN PORTD0 //Pin 2 for ATMega328P

typedef enum {DONE=0, FIRST_HALF, SECOND_HALF} SEND_STATE;

volatile SEND_STATE send_state;

void send_file(FILINFO*);
void timer0_init(void);

#endif /* KC_CASS_INTERFACE_H_ */
/*
 * kc_cass_recv_file.h
 *
 * Created: 06.06.2017 19:10:54
 *  Author: Bernd
 */ 


#ifndef KC_CASS_RECV_FILE_H_
#define KC_CASS_RECV_FILE_H_

#define CASS_IN_PORT PORTD
#define CASS_IN_DDR DDRD
#define CASS_IN_PIN PORTD2 //Pin 4 for ATMega328P

#define FILE_EXT "TAP"
#define DEFAULT_FILE_NAME "KC-DATEI" // length must be 8!

#ifdef DEBUG_RECV_TIMER
#define MONITOR_RECV_PORT PORTB
#define MONITOR_RECV_DDR DDRB
#define MONITOR_RECV_PIN PINB
#define MONITOR_RECV_BIT1 PORTB7 //Pin 10 for ATMega328P
#define MONITOR_RECV_BIT2 PORTB6 //Pin 9 for ATMega328P
//#define MONITOR_RECV_BIT3 PORTB1 //Pin 15 for ATMega328P
#define MONITOR_RECV_PIN1_LOW (MONITOR_RECV_PORT &= ~_BV(MONITOR_RECV_BIT1))
#define MONITOR_RECV_PIN1_HIGH (MONITOR_RECV_PORT |= _BV(MONITOR_RECV_BIT1))
#define MONITOR_RECV_PIN1_TOGGLE (MONITOR_RECV_PIN |= _BV(MONITOR_RECV_BIT1))
#define MONITOR_RECV_PIN2_LOW (MONITOR_RECV_PORT &= ~_BV(MONITOR_RECV_BIT2))
#define MONITOR_RECV_PIN2_HIGH (MONITOR_RECV_PORT |= _BV(MONITOR_RECV_BIT2))
#define MONITOR_RECV_PIN2_TOGGLE (MONITOR_RECV_PIN |= _BV(MONITOR_RECV_BIT2))
//#define MONITOR_RECV_PIN3_LOW (MONITOR_RECV_PORT &= ~_BV(MONITOR_RECV_BIT3))
//#define MONITOR_RECV_PIN3_HIGH (MONITOR_RECV_PORT |= _BV(MONITOR_RECV_BIT3))
//#define MONITOR_RECV_PIN3_TOGGLE (MONITOR_RECV_PIN |= _BV(MONITOR_RECV_BIT3))
#endif

void kc_cass_recv_file_init(void);
void kc_cass_recv_file_disable(void);
void kc_cass_handle_recv_file(void); // returns true if in receive state

#endif /* KC_CASS_RECV_FILE_H_ */
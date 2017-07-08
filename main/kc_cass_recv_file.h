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

void kc_cass_recv_file_init(void);
void kc_cass_recv_file_disable(void);
void kc_cass_handle_recv_file(void);

#endif /* KC_CASS_RECV_FILE_H_ */
/*
 * debounced_keys.h
 *
 * Created: 02.12.2015 11:54:28
 *  Author: Bernd
 */ 


#ifndef DEBOUNCED_KEYS_H_
#define DEBOUNCED_KEYS_H_
#include <stdbool.h>

#ifndef DEBOUNCE_COUNTER_MAX
#	define DEBOUNCE_COUNTER_MAX 15U
#endif

#define PORT_ID_PORTA 0
#define PORT_ID_PORTB 1
#define PORT_ID_PORTC 2
#define PORT_ID_PORTD 3

#define NUM_KEYS 3

#define KEY_PIN_MAP { \
	{PORT_ID_PORTB, PINB0}, \
	{PORT_ID_PORTC, PINC4}, \
	{PORT_ID_PORTC, PINC5} \
}

extern void handle_keys(void);
extern void keys_init(void);

typedef enum {STAY=0, UP, DOWN} DIRECTION;

#define SELECT_KEY 0
#define ROTARY_A SELECT_KEY + 1
#define ROTARY_B ROTARY_A + 1

extern volatile DIRECTION display_task;
extern volatile bool select_key_pressed;
extern volatile bool select_key_changed;

#endif /* DEBOUNCED_KEYS_H_ */
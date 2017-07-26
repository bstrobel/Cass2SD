/*
 * debounced_keys.c
 *
 * Created: 02.12.2015 11:54:52
 *  Author: Bernd
 */ 

#include <stdint.h>
#include "debounced_keys.h"
#include <avr/io.h>
#include <avr/sfr_defs.h>
#include <stdbool.h>

volatile DIRECTION display_task = STAY;
volatile bool select_key_pressed;
volatile bool select_key_changed;

volatile uint8_t debounce_counter[NUM_KEYS];

volatile uint8_t keys_last_state_bitmap;	// 
volatile uint8_t keys_bitmap;				// bitmap of debounced pressed/unpressed state
volatile uint8_t keys_changed_bitmap;		// bitmap indicating if the debounced state of a key has changed since last cycle

typedef struct
{
	uint8_t port_id, port_bit_num;
} key_pin;
key_pin key_pin_map[NUM_KEYS] = KEY_PIN_MAP;

void keys_init()
{
	for (int i = 0; i < NUM_KEYS; i++)
	{
		switch(key_pin_map[i].port_id)
		{
			case PORT_ID_PORTB: {
				DDRB &= ~_BV(key_pin_map[i].port_bit_num);
				PORTB |= _BV(key_pin_map[i].port_bit_num);
				break;
			}
			case PORT_ID_PORTC: {
				DDRC &= ~_BV(key_pin_map[i].port_bit_num);
				PORTC |= _BV(key_pin_map[i].port_bit_num);
				break;
			}
			case PORT_ID_PORTD: {
				DDRD &= ~_BV(key_pin_map[i].port_bit_num);
				PORTD |= _BV(key_pin_map[i].port_bit_num);
				break;
			}
		}
		debounce_counter[i] = DEBOUNCE_COUNTER_MAX + 1U;

	}
}

void handle_keys()
{
	// step through all the defined keys and update the keys_changed_bitmap
	for (int i = 0; i < NUM_KEYS; i++) {
		uint8_t new_key_val;
		switch(key_pin_map[i].port_id) {
			case PORT_ID_PORTB: {
				new_key_val = ~PINB & _BV(key_pin_map[i].port_bit_num); // PIN[key] == 0 => is pressed
				break;
			}
			case PORT_ID_PORTC: {
				new_key_val = ~PINC & _BV(key_pin_map[i].port_bit_num); // PIN[key] == 0 => is pressed
				break;
			}
			default:
			case PORT_ID_PORTD: {
				new_key_val = ~PIND & _BV(key_pin_map[i].port_bit_num); // PIN[key] == 0 => is pressed
				break;
			}
		}
		if (keys_last_state_bitmap & _BV(i)) {
			//key was pressed before
			if (new_key_val) {
				//key still pressed
				if (debounce_counter[i] < DEBOUNCE_COUNTER_MAX) {
					debounce_counter[i]++;
				}
			}
			else {
				//key now released
				debounce_counter[i]=0;
				keys_last_state_bitmap &= ~_BV(i);
			}
		}
		else {
			//key was released before
			if (new_key_val) {
				//key now pressed
				debounce_counter[i] = 0;
				keys_last_state_bitmap |= _BV(i);
			}
			else {
				//key still pressed
				if (debounce_counter[i] < DEBOUNCE_COUNTER_MAX) {
					debounce_counter[i]++;
				}
			}
		}
		if (debounce_counter[i] == DEBOUNCE_COUNTER_MAX) {
			// this means that the key has stayed in the same state for DEBOUNCE_COUNTER_MAX timer cycles.
			debounce_counter[i]++; //increase debounce_counter one last time so we dont go into this "if" again
			if (new_key_val) {
				keys_bitmap |= _BV(i);
			}
			else {
				keys_bitmap &= ~_BV(i);
			}
			keys_changed_bitmap |= _BV(i);
		}
	}
	
	
	// set global variables reflecting the status of the keys in keys_changed_bitmap
	if ((keys_changed_bitmap & _BV(SELECT_KEY)) && (~keys_bitmap & _BV(SELECT_KEY))) {
		keys_changed_bitmap &= ~_BV(SELECT_KEY);
		select_key_pressed = true;
		select_key_changed = true;
	}
	else if  ((keys_changed_bitmap & _BV(SELECT_KEY)) && !(~keys_bitmap & _BV(SELECT_KEY))) {
		keys_changed_bitmap &= ~_BV(SELECT_KEY);
		select_key_pressed = false;
		select_key_changed = true;
	}
	// check for adjustment (rotary encoder)
	if (keys_changed_bitmap & _BV(ROTARY_A)) {
		keys_changed_bitmap &= ~_BV(ROTARY_A);
		if (!(keys_bitmap & _BV(ROTARY_A))) { // A changed to L
			if (keys_bitmap & _BV(ROTARY_B)) { // and B is H
				display_task=UP;
			}
		}
	}
	if (keys_changed_bitmap & _BV(ROTARY_B)) {
		keys_changed_bitmap &= ~_BV(ROTARY_B);
		if (!(keys_bitmap & _BV(ROTARY_B))) { // B changed to L
			if (keys_bitmap & _BV(ROTARY_A)) {// and A is H
				display_task=DOWN;
			}
		}
	}
}

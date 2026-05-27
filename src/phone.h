#ifndef PHONE_H
#define PHONE_H

#include <avr/io.h>
#include <avr/interrupt.h>
#include <avr/eeprom.h>
#include <util/atomic.h>
#include <string.h>

#include "uart.h"
#include "mem.h"
#include "ringtone.h"

#define MAX_LEN_PHONE_NUMBER 20

extern volatile uint32_t millis_count;
extern volatile uint8_t digit;
extern volatile uint8_t phone_number[MAX_LEN_PHONE_NUMBER];
extern volatile uint8_t len_phone_number;
extern volatile uint8_t command[MAX_LEN_PHONE_NUMBER + 2];
extern volatile uint8_t len_command;

extern volatile uint8_t receiver_down;
extern volatile uint8_t last_receiver_state;
extern volatile uint8_t in_call;
extern volatile uint32_t last_dial_millis;

extern volatile uint8_t last_PINB;
extern volatile uint8_t last_PD0;
extern volatile uint8_t last_PD1;

extern volatile uint8_t ringing;
extern volatile uint32_t last_ring_time;
extern volatile uint8_t bell_pulse_timer;

extern uint8_t current_ringtone_idx;
extern uint8_t play_ringtone_demo;


uint32_t get_time();
uint32_t get_last_dial_time();
void init_timer1();
void init_disc();
void init_receiver();
void init_sim800l();
void init_bell();
void init_ringtone();

#endif

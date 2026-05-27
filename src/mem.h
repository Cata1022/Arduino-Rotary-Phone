#ifndef EEPROM_H
#define EEPROM_H

#include <avr/eeprom.h>
#include "phone.h"

extern uint8_t fav_phone_numbers[10][20] EEMEM;
extern uint8_t len_fav_phone_numbers[10] EEMEM;
extern uint8_t idx_ringtone EEMEM;

// Stores a phone number + its length in eeprom
void mem_store_number(uint8_t idx_number, volatile uint8_t* number, uint8_t len_number);

// Loads a phone number from eeprom in to the dedicated array for dialing
uint8_t mem_load_number(uint8_t idx_number);

// Changes the ringtone index from eeprom
void mem_change_ringtone(uint8_t idx_new_ringtone);

// Returns the ringtone index from eeprom
uint8_t mem_get_ringtone_idx();

#endif

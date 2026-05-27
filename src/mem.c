#include "mem.h"

uint8_t fav_phone_numbers[10][20] EEMEM;
uint8_t len_fav_phone_numbers[10] EEMEM;
uint8_t idx_ringtone EEMEM;

// Stores a phone number + its length in eeprom
void mem_store_number(uint8_t idx_number, volatile uint8_t* number, uint8_t len_number) {
	eeprom_update_byte(&(len_fav_phone_numbers[idx_number]), len_number);

	eeprom_update_block (
        (const void*)number, 
        (void*)&(fav_phone_numbers[idx_number]), 
        len_number
    );
}

// Loads a phone number from eeprom in to the dedicated array for dialing
uint8_t mem_load_number(uint8_t idx_number) {
	uint8_t len_number = eeprom_read_byte(&(len_fav_phone_numbers[idx_number]));

    // Checks for valid number length
    if (len_number == 0 || len_number > 20)
        return 0;

    len_phone_number = len_number;
    eeprom_read_block (
        (void*)phone_number, 
        (const void*)&(fav_phone_numbers[idx_number]), 
        len_phone_number
    );

	// Valid number length
	return 1;
}

// Changes the ringtone index from eeprom
void mem_change_ringtone(uint8_t idx_new_ringtone) {
	eeprom_update_byte(&idx_ringtone, idx_new_ringtone);
}

// Returns the ringtone index from eeprom
uint8_t mem_get_ringtone_idx() {
	return eeprom_read_byte(&idx_ringtone);
}

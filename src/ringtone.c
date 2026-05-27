#include "ringtone.h"

// Ringtones represented by the duration of the pauses between notes in ms
const uint16_t classic[] PROGMEM = {47, 47, 47, 47, 47, 47, 47, 47, 47, 47, 47, 47, 47, 47, 47, 47, 47, 47, 47};
const uint16_t mozart[] PROGMEM = {120, 120, 120, 120, 480, 120, 120, 120, 120, 480, 120, 120, 120, 120, 120, 120, 120, 120, 120, 120, 120, 120, 480, 240, 240, 60, 60, 120, 240, 240, 240, 60, 60, 120, 240, 240, 240, 60, 60, 120, 240, 240, 240, 480, 120, 60, 120, 120, 480, 120, 120, 120, 120, 480, 120, 120, 120, 120, 120, 120, 120, 120, 120, 120, 120, 120, 480, 240, 240, 60, 60, 120, 240, 240, 240, 60, 60, 120, 240, 240, 240, 60, 60, 120, 240, 240, 240};
const uint16_t mission_impossible[] PROGMEM = {75, 75, 75, 75, 75, 75, 75, 75, 75, 75, 75, 75, 75, 75, 75, 450, 450, 300, 300, 450, 450, 300, 300, 450, 450, 300, 300, 450, 450, 300, 300, 150, 150, 1275, 150, 150, 1275, 150, 150, 1350, 150};
const uint16_t lucky_discovery[] PROGMEM = {214, 214, 107, 107, 107, 107, 214, 214, 214, 214, 107, 107, 107, 107, 214, 214, 214, 214, 107, 107, 107, 107, 214, 214, 214, 214, 214, 214};
const uint16_t nokia_tune[] PROGMEM = {150, 150, 300, 300, 150, 150, 300, 300, 150, 150, 300, 400};
const uint16_t jingle_bells[] PROGMEM = {353, 353, 706, 353, 353, 706, 353, 353, 529, 176, 1235, 353, 353, 353, 176, 353, 353, 353, 176, 176, 353, 353, 353, 353, 706};

// Ringtones array
const uint16_t* const ringtones[] PROGMEM = {
	classic,
    jingle_bells,
	mozart,
	lucky_discovery,
	mission_impossible,
    nokia_tune
};

const uint8_t len_ringtones[] PROGMEM = {19, 25, 87, 28, 41, 12};

static uint8_t note_idx = 0;
static uint32_t wait_time = 0;
static uint32_t last_action_time = 0;


void reset_ringtone() {
	PORTB &= ~(1 << PB6);

    note_idx = 0;
    wait_time = 0;
    last_action_time = 0;
    
    bell_pulse_timer = 0;  
}

void play_ringtone() {
    uint32_t current_time = get_time();

    // Wait for the pause between notes
	if (current_time - last_action_time >= wait_time) {
        
        uint8_t ringtone_idx = current_ringtone_idx;

        uint8_t len_ringtone = pgm_read_byte(&(len_ringtones[ringtone_idx]));
        const uint16_t* ringtone = (const uint16_t*)pgm_read_ptr(&(ringtones[ringtone_idx]));

        if (note_idx < len_ringtone) {
			// Load the next pause time between notes
            wait_time = pgm_read_word(&(ringtone[note_idx]));
            note_idx++;
        } else {
			// Pause for 3 secs before repeating the ringtone
            wait_time = 3000;
            note_idx = 0;

			// Stop playing the ringtone demo
			if (play_ringtone_demo) {
				play_ringtone_demo = 0;
				uart_println("AT+GSMBUSY=0");
				reset_ringtone();
			}
        }

        // Wait between 3ms and 4ms
        bell_pulse_timer = 4;
		// Pull the hammer
        PORTB |= (1 << PB6);

        last_action_time = current_time;
    }
}

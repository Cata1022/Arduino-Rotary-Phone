#include <Arduino.h>
#include <avr/io.h>
#include <avr/interrupt.h>
#include <util/atomic.h>
#include <string.h>

#include "uart.h"

#define MAX_LEN_PHONE_NUMBER 20

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


volatile uint32_t millis_count = 0;

volatile uint8_t digit = 0;

volatile uint8_t phone_number[MAX_LEN_PHONE_NUMBER];
volatile uint8_t len_phone_number = 0;

volatile uint8_t command[MAX_LEN_PHONE_NUMBER + 2];
volatile uint8_t len_command = 0;

volatile uint8_t receiver_down;
volatile uint8_t last_receiver_state;
volatile uint8_t in_call = 0;

volatile uint32_t last_dial_millis = 0;

volatile uint8_t last_PINB;
volatile uint8_t last_PD0;
volatile uint8_t last_PD1;

volatile uint8_t ringing = 0;
volatile uint32_t last_ring_time = 0;

volatile uint8_t bell_pulse_timer = 0;


void init_timer1() {
	TCCR1A = 0; 
    TCCR1B = 0;
	OCR1A = 249;
	TCCR1B |= (1 << WGM12) | (1 << CS11) | (1 << CS10);
    TIMSK1 |= (1 << OCIE1A);
}

void init_disc() {
	// Input for disc pulses
    DDRD &= ~(1 << PD0);
    // Pull up the disc pulse line
    PORTD |= (1 << PD0);

	// Input for disc rotation detection
	DDRD &= ~(1 << PD1);
	// Pull up the rotation detection line
	PORTD |= (1 << PD1); 

    // Interrupt for disc pulses
    EIMSK |= (1 << INT0);
    EICRA &= ~(1 << ISC01); 
    EICRA |= (1 << ISC00);

	// Interrupt for disc rotation
	EIMSK |= (1 << INT1);
    EICRA &= ~(1 << ISC11); 
    EICRA |= (1 << ISC10);

	last_PD0 = (PIND & (1 << PD0)) ? 1 : 0;
	last_PD1 = (PIND & (1 << PD1)) ? 1 : 0;
}

void init_receiver() {
	// Input for receiver
	DDRB &= ~(1 << PB1);
	// Pull up for receiver
	PORTB |= (1 << PB1);

	// Receiver interrupt
	PCICR |= (1 << PCIE0);
	PCMSK0 |= (1 << PCINT1);
	
	last_PINB = PINB;
	receiver_down = (last_PINB & (1 << PB1)) ? 1 : 0;
	last_receiver_state = receiver_down;

	// When receiver is up no calls are allowed
	if (!receiver_down) {
		uart_println("AT+GSMBUSY=1");
	} else {
		uart_println("AT+GSMBUSY=0");
	}
}

void init_sim800l() {
	// Establish conection and baud rate
	uart_println("AT");
	uart_println("AT");
	uart_println("AT");

	uart_println("AT+CLVL=15");
	uart_println("AT+CMIC=0,7");
	uart_println("AT+ECHO=0,96,253,16388,20488,1");

	uart_println("AT+CRSL=0");
}

void init_bell() {
	PORTB &= ~(1 << PB6);
	DDRB |= (1 << PB6);
}


// Milliseconds timer and bell pulse control
ISR(TIMER1_COMPA_vect) {
    millis_count++;

	if (bell_pulse_timer > 0) {
        bell_pulse_timer--;
        if (bell_pulse_timer == 0) {
			// Release the hammer
            PORTB &= ~(1 << PB6);
        }
    }
}

// PB1 (D15) -> Detects if the reciever is down on the stand or up
// PB1 = HIGH => receiver down / PB1 = LOW => receiver up
ISR(PCINT0_vect) {
	uint8_t current_PINB = PINB;
	uint8_t changed_pins = current_PINB ^ last_PINB;

	static uint32_t last_receiver_change = 0;

	if (changed_pins & (1 << PB1) && (millis_count - last_receiver_change >= 50)) {
		receiver_down = (current_PINB & (1 << PB1)) ? 1 : 0;

		if (receiver_down)
			len_phone_number = 0;
		else
			len_command = 0;

		last_receiver_change = millis_count;
	}

	last_PINB = current_PINB;
}

// PD0 (D3) -> Detects pulses from the disc to decode the input digit
// Digit N = N pulses (LOW -> HIGH)
ISR(INT0_vect) {
	uint8_t current_PD0 = (PIND & (1 << PD0)) ? 1 : 0;

	static uint32_t last_pin_change = 0;

	if (current_PD0 != last_PD0 && (millis_count - last_pin_change > 20)) {
		// Pulse detected
		if (current_PD0 == 1)
			digit++;
		
		last_pin_change = millis_count;
	}

	last_PD0 = current_PD0;
}

// PD1 (D2) -> Detects when the disc stops rotating, indicating a digit
// has been dialed; HIGH = disc stopped
ISR(INT1_vect) {
	uint8_t current_PD1 = (PIND & (1 << PD1)) ? 1 : 0;

	static uint32_t last_pin_change = 0;

	if (current_PD1 != last_PD1 && (millis_count - last_pin_change > 40)) {
		// Disc stopped rotating, valid digit was dialed
		if (current_PD1 == 1 && digit > 0) {
			if (digit == 10)
				digit = 0;
			
			if (receiver_down) {
				// Dialaing a command
				if (len_command < 22)
					command[len_command++] = digit;
			} else {
				// Dialing a phone number
				if (len_phone_number < 20)
					phone_number[len_phone_number++] = digit;
			}

			digit = 0;
			last_dial_millis = millis_count;
		}
		
		last_pin_change = millis_count;
	}

	last_PD1 = current_PD1;
}


uint32_t get_time() {
	uint32_t time;
	ATOMIC_BLOCK(ATOMIC_RESTORESTATE) {
		time = millis_count;
	}
	return time;
}

uint32_t get_last_dial_time() {
	uint32_t last_dial_time;
	ATOMIC_BLOCK(ATOMIC_RESTORESTATE) {
		last_dial_time = last_dial_millis;
	}
	return last_dial_time;
}

// In ringtone.h
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
        
        uint8_t ringtone_idx = 0; // Get idx from EEPROM (TODO)

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
        }

        // Wait between 3ms and 4ms
        bell_pulse_timer = 4;
		// Pull the hammer
        PORTB |= (1 << PB6);

        last_action_time = current_time;
    }
}


void setup() {
	init_uart();
	init_bell();
	init_timer1();
	init_disc();
	init_receiver();
	init_sim800l();

    sei();

    Serial.begin(9600);
}


// In main (cred)
char sim_buffer[32];
uint8_t sim_idx = 0;

void loop() {
	// Read SIM800L output
	while (uart_available()) {
        char c = uart_read();
        
        if (c == '\n' || c == '\r') {
            if (sim_idx > 0) {
                sim_buffer[sim_idx] = '\0'; 
                
                if (strstr(sim_buffer, "RING") != NULL) {
                    ringing = 1;
                    last_ring_time = get_time();
					Serial.println("RING");
                } 
                else if (strstr(sim_buffer, "NO CARRIER") != NULL) {
                    ringing = 0;
					in_call = 0;
                    reset_ringtone();
					uart_println("ATH");
					Serial.println("NO CARRIER");
                }
                
                sim_idx = 0; 
            }
        } 
        else {
            if (sim_idx < sizeof(sim_buffer) - 1) {
                sim_buffer[sim_idx++] = c;
            }
        }
    }

	// A phone number has been dialed
	if (len_phone_number > 0 && (get_time() - get_last_dial_time()) > 4000) {
		in_call = 1;

		uart_print("ATD+");
		Serial.print("ATD+"); //
		for (uint8_t i = 0; i < len_phone_number; i++) {
			uart_print_char(phone_number[i] + '0');
			Serial.print(phone_number[i]); //
		}
		uart_println(";");
		Serial.println(";"); //

		len_phone_number = 0;
	}

	// Change of receiver state
	if (receiver_down != last_receiver_state) {
		if (receiver_down) {
			// Receive calls
			uart_println("AT+GSMBUSY=0");

			// Receiver down while in call => hang up
			if (in_call) {
				in_call = 0;
				uart_println("ATH");
			}
		} else {
			// Receiver up while phone is ringing => answer
			if (ringing) {
				ringing = 0;
				in_call = 1;
				uart_println("ATA");
				reset_ringtone();
			} else {
				// Do not receive calls
				uart_println("AT+GSMBUSY=1");
			}
		}

		last_receiver_state = receiver_down;
	}

	// If phone is ringing play the current ringtone
	if (ringing && receiver_down) {
		play_ringtone();
	}

	// Stop ringing if no RING signal is received for more than 8 secs
	if (ringing && (get_time() - last_ring_time > 4500)) {
        ringing = 0;
        reset_ringtone();
    }

	// A command has been given
	if (len_command > 1 && (get_time() - get_last_dial_time()) > 4000) {
		if (command[0] == 2 && command[1] == 1) {
			Serial.println("Comanda");
		}

		len_command = 0;
	}
}



// #include <Arduino.h>
// #include <avr/io.h>
// #include <util/delay.h>


// // Classic
// uint16_t melodie[] = {47, 47, 47, 47, 47, 47, 47, 47, 47, 47, 47, 47, 47, 47, 47, 47, 47, 47, 47};
// uint8_t lungime_melodie = 19;

// // Mozart
// // uint16_t melodie[] = {120, 120, 120, 120, 480, 120, 120, 120, 120, 480, 120, 120, 120, 120, 120, 120, 120, 120, 120, 120, 120, 120, 480, 240, 240, 60, 60, 120, 240, 240, 240, 60, 60, 120, 240, 240, 240, 60, 60, 120, 240, 240, 240, 480, 120, 60, 120, 120, 480, 120, 120, 120, 120, 480, 120, 120, 120, 120, 120, 120, 120, 120, 120, 120, 120, 120, 480, 240, 240, 60, 60, 120, 240, 240, 240, 60, 60, 120, 240, 240, 240, 60, 60, 120, 240, 240, 240};
// // uint8_t lungime_melodie = 87;

// // Mission Impossible
// // uint16_t melodie[] = {75, 75, 75, 75, 75, 75, 75, 75, 75, 75, 75, 75, 75, 75, 75, 450, 450, 300, 300, 450, 450, 300, 300, 450, 450, 300, 300, 450, 450, 300, 300, 150, 150, 1275, 150, 150, 1275, 150, 150, 1350, 150};
// // uint8_t lungime_melodie = 41;

// // Lucky Discovery
// // uint16_t melodie[] = {214, 214, 107, 107, 107, 107, 214, 214, 214, 214, 107, 107, 107, 107, 214, 214, 214, 214, 107, 107, 107, 107, 214, 214, 214, 214, 214, 214};
// // uint8_t lungime_melodie = 28;

// // Nokia Tune
// // uint16_t melodie[] = {150, 150, 300, 300, 150, 150, 300, 300, 150, 150, 300, 400};
// // uint8_t lungime_melodie = 12;

// // Jingle Bells
// // uint16_t melodie[] = {353, 353, 706, 353, 353, 706, 353, 353, 529, 176, 1235, 353, 353, 353, 176, 353, 353, 353, 176, 176, 353, 353, 353, 353, 706};
// // uint8_t lungime_melodie = 25;


// int stare_citire = 0; // Urmărește literele primite în ordine

// void setup() {
//     // Punem pinul pe LOW înainte să îl facem output pentru siguranță
//     PORTB &= ~(1 << PB6); 
//     DDRB |= (1 << PB6);   

//     Serial.begin(9600);
// }

// void loop() {
//     // Cât timp avem caractere noi de la PC
//     while (Serial.available() > 0) {
//         char c = Serial.read();

//         if (stare_citire == 0 && c == 'o') {
//             stare_citire = 1; // Am primit 'o', trecem la pasul următor
//         } 
//         else if (stare_citire == 1 && c == 'n') {
//             stare_citire = 2; // Am primit 'n', așteptăm Enter
//         } 
//         else if (stare_citire == 2 && (c == '\n' || c == '\r')) {
//             // Am primit secvența perfectă: "on" + Enter. 
//             // DECLANȘĂM INSTANT!
            
//             //noInterrupts();

// 			for (int i = 0; i < lungime_melodie; i++) {
// 				PORTB |= (1 << PB6);
// 				_delay_ms(3);
// 				PORTB &= ~(1 << PB6);
// 				delay(melodie[i]);
// 			}
// 			PORTB |= (1 << PB6);
// 			_delay_ms(3);
// 			PORTB &= ~(1 << PB6);

//             //interrupts();
            
//             Serial.println("Puls repetat.");
//             stare_citire = 0; // Resetăm starea pentru următoarea comandă
//         } 
//         else if (c != '\n' && c != '\r') {
//             // Orice altă literă/greșeală de tastare resetează sistemul instant
//             stare_citire = 0;
//         }
//     }
// }



// #include <Arduino.h>

// void setup() {
// 	Serial.begin(9600);
// 	delay(3000); 
// 	Serial.println("Sistem pornit! Initializam conexiunea hardware...");

// 	Serial1.begin(9600);
// 	delay(1000); 
// 	Serial.println("Gata! Tot ce scrii aici va fi trimis catre SIM800L.");
// }

// void loop() {
// 	if (Serial.available()) {
// 	char c = Serial.read();
// 	Serial1.write(c);
// 	}

// 	if (Serial1.available()) {
// 	char c = Serial1.read();
// 	Serial.write(c);
// 	}
// }



// #include <Arduino.h>

// void setup() {
// 	Serial.begin(9600);
// 	delay(3000); 
// 	Serial.println("Sistem pornit! Initializam conexiunea hardware...");

// 	Serial1.begin(9600);
// 	delay(1000); 
// 	Serial.println("Gata! Tot ce scrii aici va fi trimis catre SIM800L.");
// }

// void loop() {
// 	if (Serial.available()) {
// 		char c = Serial.read(); 

// 		if (c == '~') {
// 			Serial1.write(26);
// 		} else {
// 			Serial1.write(c);
// 		}
// 	}

// 	if (Serial1.available()) {
// 		char c = Serial1.read();
// 		Serial.write(c); 
// 	}
// }

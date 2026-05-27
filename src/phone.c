#include "phone.h"

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

uint8_t current_ringtone_idx;
uint8_t play_ringtone_demo = 0;

// ISRs

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
}

void init_sim800l() {
    uint8_t sim_ready = 0;
    char temp_buf[16];
    uint8_t t_idx = 0;

	// Waits for SIM800L to start and respond back
    while (!sim_ready) {
		// Initiate communication
        uart_println("AT");
        
        // Wait 500 ms for a response
        uint32_t start_wait = get_time();
        while (get_time() - start_wait < 500) {
            while (uart_available()) {
                char c = uart_read();
                if (c == '\n' || c == '\r') {
                    if (t_idx > 0) {
                        temp_buf[t_idx] = '\0';
						// Communication with module established
                        if (strstr(temp_buf, "OK") != NULL) {
                            sim_ready = 1;
                        }
                        t_idx = 0;
                    }
                } else {
                    if (t_idx < sizeof(temp_buf) - 1) {
                        temp_buf[t_idx++] = c;
                    }
                }
            }
            if (sim_ready) break;
        }
    }

    // MIC and SPK settings
    uart_println("AT+CLVL=15");
    uart_println("AT+CMIC=0,7");
	uart_println("AT+ECHO=0,96,253,16388,20488,1");
    uart_println("AT+CRSL=0");

    // Receive calls only when receiver is down
	if (receiver_down) {
        uart_println("AT+GSMBUSY=0"); 
    } else {
        uart_println("AT+GSMBUSY=1");
    }
}

void init_bell() {
	PORTB &= ~(1 << PB6);
	DDRB |= (1 << PB6);
}

void init_ringtone() {
	current_ringtone_idx = mem_get_ringtone_idx();
	
	// Default ringtone value
	if (current_ringtone_idx >= RINGTONES_NO) {
		mem_change_ringtone(0);
		current_ringtone_idx = 0;
	}
}

#include "phone.h"

int main() {
	init_uart();
	init_bell();
	init_timer1();
	init_disc();
	init_receiver();
	init_ringtone();

	sei();

	init_sim800l();

	char sim_buffer[32];
	uint8_t sim_idx = 0;

	while (1) {
		// Read SIM800L output
		while (uart_available()) {
			char c = uart_read();
			
			if (c == '\n' || c == '\r') {
				if (sim_idx > 0) {
					sim_buffer[sim_idx] = '\0'; 
					
					if (strstr(sim_buffer, "RING") != NULL) {
						ringing = 1;
						last_ring_time = get_time();
					} 
					else if (strstr(sim_buffer, "NO CARRIER") != NULL) {
						ringing = 0;
						in_call = 0;
						reset_ringtone();
						uart_println("ATH");
						if (!receiver_down)
							uart_println("AT+GSMBUSY=1");
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
		if (len_phone_number > 0 && (get_time() - get_last_dial_time()) > 4000 && !receiver_down) {
			in_call = 1;

			uart_print("ATD+");
			for (uint8_t i = 0; i < len_phone_number; i++) {
				uart_print_char(phone_number[i] + '0');
			}
			uart_println(";");

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

				// If the demo was playing, stop it
				if (play_ringtone_demo) {
					play_ringtone_demo = 0;
					reset_ringtone();
				}
			}

			last_receiver_state = receiver_down;
		}

		// If phone is ringing play the current ringtone
		if ((ringing || play_ringtone_demo) && receiver_down) {
			play_ringtone();
		}

		// Stop ringing if no RING signal is received for more than 4.5 secs
		if (ringing && (get_time() - last_ring_time > 4500)) {
			ringing = 0;
			reset_ringtone();
		}

		// Commands
		if (len_command > 0) {

			if (len_command == 2 && (command[0] == 1 || command[0] == 3)) {
				// Do not receive calls while executing commands
				ringing = 0;
				uart_println("AT+GSMBUSY=1");
				
				// Call a favourite number
				if (command[0] == 1) {
					uint8_t idx_number = command[1];
					
					// Invalid number
					if(!mem_load_number(idx_number)) {
						uart_println("AT+GSMBUSY=0");
						len_phone_number = 0;
					}
				}
				
				// Change the ringtone
				else if (command[0] == 3) {
					uint8_t idx_new_ringtone = command[1];

					if (idx_new_ringtone >= RINGTONES_NO)
						idx_new_ringtone = 0;

					mem_change_ringtone(idx_new_ringtone);
					current_ringtone_idx = idx_new_ringtone;

					play_ringtone_demo = 1;
				}

				len_command = 0;
			
			} else if ((get_time() - get_last_dial_time()) > 4000) {
				
				// Store a new favourite number
				if (len_command > 2 && command[0] == 2) {
					uint8_t idx_number = command[1];
					mem_store_number(idx_number, command + 2, len_command - 2);
				} 

				len_command = 0;
			}
		}
	}
}

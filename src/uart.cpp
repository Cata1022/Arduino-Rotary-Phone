#include "uart.h"

#ifndef F_CPU
    #define F_CPU 16000000UL
#endif

#define BAUD 9600
#define UBRR_VALUE ((F_CPU / 16 / BAUD) - 1)


void init_uart() {
    UBRR1H = (uint8_t)(UBRR_VALUE >> 8);
    UBRR1L = (uint8_t)UBRR_VALUE;
    
    UCSR1B = (1 << TXEN1) | (1 << RXEN1);
    
    UCSR1C = (1 << UCSZ11) | (1 << UCSZ10);
}

uint8_t uart_available() {
    return (UCSR1A & (1 << RXC1)) ? 1 : 0;
}

char uart_read() {
    return UDR1;
}

void uart_print_char(char c) {
    while (!(UCSR1A & (1 << UDRE1)));
    UDR1 = c;
}

void uart_print(const char* str) {
    while (*str) {
        while (!(UCSR1A & (1 << UDRE1)));
        UDR1 = *str++;
    }
}

void uart_println(const char* str) {
    uart_print(str);
    uart_print("\r\n");
}

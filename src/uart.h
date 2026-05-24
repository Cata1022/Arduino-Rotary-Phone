#ifndef UART_H
#define UART_H

#include <avr/io.h>

void init_uart();
uint8_t uart_available();
char uart_read();
void uart_print_char(char c);
void uart_print(const char* str);
void uart_println(const char* str);

#endif

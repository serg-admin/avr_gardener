#include <avr/io.h>
#include <avr/interrupt.h>
#include <avr/sleep.h>
#include "uart_async.h"

unsigned char uart_buf[UART0_BUFER_SIZE];
char uart_read_buf[UART0_READ_BUFER_SIZE];
unsigned char uart_read_wPos = 0;
unsigned char uart_wPos = 0; // Позиция буфера для записи новых данных.
unsigned char uart_rPos = 0; // Позиция буфера для передачи в порт.
void (*uart_readln_callback)(char*) = 0;


ISR (USART_UDRE_vect) {
  cli();
  if (uart_wPos != uart_rPos) {
    UDR0 = uart_buf[uart_rPos++];
    if (uart_rPos >= UART0_BUFER_SIZE) uart_rPos = 0;
  } else UCSR0B &= ~(_BV(UDRIE0));
  sei();
}

// Прерывание - Пришел байт данных.
ISR (USART_RX_vect) {
  if (uart_readln_callback == 0) return;
  PORTB ^= _BV(PINB5);
  cli();
  while (UCSR0A & _BV(RXC0)) {
    unsigned char t = UDR0; // Из порта байт можно прочитать только один раз.
    if ((t == 0x0A) || (t == 0x0D)) {
      uart_read_buf[uart_read_wPos] = 0; //Конец строки
      uart_readln_callback(uart_read_buf);
      uart_read_wPos = 0;
    } else {
      uart_read_buf[uart_read_wPos++] = t;
    }
  }
  sei();
}

void uart_readln(void (*callback)(char*)) {
  uart_readln_callback = callback;
}

void uart_async_init(void) {
   uart_wPos = 0;
   uart_rPos = 0;
   // Разрешить прием, передачу через порт.
   UCSR0B = _BV(TXEN0) | _BV(RXEN0) | _BV(RXCIE0) ;
   // Устанавливаем скорость порта.
   UBRR0 = MYBDIV;
   UCSR0C = _BV(UCSZ01) | _BV(UCSZ00);
}

// Возвращает количество свободных байт в очереди USART.
char uart_getBufSpace() {
  if (uart_rPos <= uart_wPos) {
    return UART0_BUFER_SIZE - uart_wPos + uart_rPos - 1;
  } else {
    return uart_rPos - uart_wPos - 1;
  }
}

// Отправляет один байт в очередь USART. В случае если очередь занята - ждет.
void uart_putChar(char c) {
  if (uart_getBufSpace() == 0) return;
  UCSR0B &= ~(_BV(UDRIE0));
  uart_buf[uart_wPos++] = c;
  if (uart_wPos >= UART0_BUFER_SIZE) uart_wPos = 0;
  if (UCSR0A & _BV(UDRE0)){
    if (uart_wPos != uart_rPos) {
      UDR0 = uart_buf[uart_rPos++];
      if (uart_rPos >= UART0_BUFER_SIZE) uart_rPos = 0;
    }
  }
  UCSR0B |= _BV(UDRIE0);
}

// Отправляет 0 терменированную строку в очередь USART.
void uart_write(char* s) {
  unsigned char i = 0;
  while(s[i] != 0) {
    uart_putChar(s[i++]);
  }
}

// Отправляет строку в очередь USART. В случае если очередь занята - ждет.
void uart_writeln(char* s) {
    uart_write(s);
    uart_write("\n\r");
}

char uart_halfchar_to_hex(unsigned char c) {
  switch (c) {
    case 0 : return '0';
    case 1 : return '1';
    case 2 : return '2';
    case 3 : return '3';
    case 4 : return '4';
    case 5 : return '5';
    case 6 : return '6';
    case 7 : return '7';
    case 8 : return '8';
    case 9 : return '9';
    case 10 : return 'A';
    case 11 : return 'B';
    case 12 : return 'C';
    case 13 : return 'D';
    case 14 : return 'E';
    case 15 : return 'F';
  }
  return 0;
}

void _uart_writeHEX(unsigned char c) {
  char r[3];
  r[0] = uart_halfchar_to_hex((c & 0xF0) / 16);
  r[1] = uart_halfchar_to_hex(c & 0x0F) ;
  r[2] = 0;
  uart_write(r);
}

void uart_writelnHEXEx(unsigned char* c, unsigned char size) {
  char r[3];
  unsigned char i;
  r[0] = '0';
  r[1] = 'x';
  r[2] = 0;
  uart_write(r);
  for(i=0; i < size; i++) {
    _uart_writeHEX(c[i]);
    uart_write(" ");
  }
  uart_writeln(&r[2]);
}

void uart_writelnHEX(unsigned char c) {
  char r[5];
  r[0] = '0';
  r[1] = 'x';
  r[2] = uart_halfchar_to_hex((c & 0xF0) / 16);
  r[3] = uart_halfchar_to_hex(c & 0x0F) ;
  r[4] = 0;
  uart_writeln(r);
}

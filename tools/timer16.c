#include "timer16.h"
#include <avr/io.h>
#include <avr/interrupt.h>
#include <avr/sleep.h>
#include "uart_async.h"
#include "common.h"
//Настройки магающей лампочки
#define ledOn PORTB |= _BV(PINB5)  
#define ledOff PORTB &= ~(_BV(PINB5))
#define ledSw PORTB ^= _BV(PINB5)

// Смещение таймера для переполнения через секунду
#define TCNT_MIN 3036

//Прерывания компараторов таймера
#define TIMER1_A_CHECK (TIMSK1 & _BV(OCIE1A))
#define TIMER1_A_EN TIMSK1 |= _BV(OCIE1A)
#define TIMER1_A_DIS TIMSK1 &= ~(_BV(OCIE1A))
#define TIMER1_B_CHECK (TIMSK1 & _BV(OCIE1B))
#define TIMER1_B_EN TIMSK1 |= _BV(OCIE1B)
#define TIMER1_B_DIS TIMSK1 &= ~(_BV(OCIE1B))

struct rec_timerTask {
  void (*func)(byte* params);
  byte* data;
} A, B;

void timer1PutTask(uint16_t delay, void (*func)(byte*), byte* data) {
  //uart_writelnHEX(TIMSK1);
  while (1) { // Буду спать здесь пока не активирую прерывание
    if (! TIMER1_A_CHECK) {
      A.func = func;
      A.data = data;
      delay = TCNT1 + delay;
      if (delay < TCNT_MIN) delay += TCNT_MIN;
      OCR1A = delay;
      TIMER1_A_EN;
      //uart_writelnHEX(TIMSK1);
      return;
    }
    if (! TIMER1_B_CHECK) {
      B.func = func;
      B.data = data;
      delay = TCNT1 + delay;
      if (delay < TCNT_MIN) delay += TCNT_MIN;
      OCR1B = delay;
      TIMER1_B_EN;
      return;
    }
    sleep_mode();
  }
}

ISR (TIMER1_COMPA_vect) {
  A.func(A.data);
  TIMER1_A_DIS;
}

ISR (TIMER1_COMPB_vect) {
  B.func(B.data);  
  TIMER1_B_DIS;
}

// Прерывание переполнения таймера
ISR (TIMER1_OVF_vect) {
  static byte timer1_position = 0; // Переключатель задач таймера
  cli();
  TCNT1 += 3036; // Корректировка времени срабатывания преполнения к одной секунде.
  ledSw;
  /*switch (timer1_position) {
    case 0 : 
      commands_reciver("I02D00002D107");
      break;
  }*/
  timer1_position++;
  if (timer1_position > 59) timer1_position = 0;
  sei();
}

void timer_init() {
  // Делитель счетчика 256 (CS10=0, CS11=0, CS12=1).
  // 256 * 65536 = 16 777 216 (тактов)
  TCCR1B |= _BV(CS12);
  //TCCR1B |= _BV(CS10); //Включить для мигания  4 -ре секунды
  // Включить обработчик прерывания переполнения счетчика таймера.
  TIMSK1 = _BV(TOIE1);
  // PRR &= ~(_BV(PRTIM1));
}
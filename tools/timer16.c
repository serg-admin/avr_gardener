#include "timer16.h"
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

void timer1IncrementCurrentTime() {
  if ((++zs042_seconds) > 59) {
    zs042_seconds = 0;
    if ((++current_time.minut) > 59) {
      current_time.minut = 0;
      if ((++current_time.hour) > 23) {
        current_time.hour = 0;
        if ((++current_time.dayOfWeek) > 7) current_time.dayOfWeek = 1;
        current_time.dayOfMonth++;
        if ((current_time.dayOfMonth > 31) || 
            ((current_time.dayOfMonth > 30) && ((current_time.month == 4) || (current_time.month == 6) || 
                                                (current_time.month == 9) || (current_time.month == 11)) ) ||
            ((current_time.dayOfMonth > 29) && (current_time.month == 2))) {
          if ((++current_time.month) > 12) current_time.month = 1;            
        }
      }
    }
  }
    
}

void timer1PutTask(uint16_t delay, void (*func)(byte*), byte* data) {
  //uart_writelnHEX(TIMSK1);
  while (1) { // Буду спать здесь пока не активирую прерывание
    if (! TIMER1_A_CHECK) {
      A.func = func;
      A.data = data;
      delay = TCNT1 + delay;
      if (delay < TCNT_MIN) delay += TCNT_MIN;
      TIFR1 |= _BV(OCF1A);
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

byte timer1RefreshTimeCallBack(unsigned char result) {
  switch(result) {
    case TW_MR_DATA_NACK : //Результат получен
      zs042_seconds = bcdToDec(commandI2CData.reciveBuf[0]);
      current_time.minut = bcdToDec(commandI2CData.reciveBuf[1]);
      current_time.hour = bcdToDec(commandI2CData.reciveBuf[2]);
      current_time.dayOfWeek = bcdToDec(commandI2CData.reciveBuf[3]);
      current_time.dayOfMonth = bcdToDec(commandI2CData.reciveBuf[4]);
      current_time.month = bcdToDec(commandI2CData.reciveBuf[5]);
      break;
    default : 
      uart_write("ERROR ");
      uart_writelnHEX(result);
  }
  return 0;
}

void timer1RefreshTime(void) {
  // Отправка адреса для чтения
  commandI2CData.data[0] = 2; // Размер блока установки адреса
  commandI2CData.data[1] = DS3231_ADDRESS; // Адресс для записи
  commandI2CData.data[2] = 0;
  // Блок чтения с I2C
  commandI2CData.data[3] = 2; // Размер блока чтения
  commandI2CData.data[4] = DS3231_ADDRESS + 1; // Адресс для чтения
  commandI2CData.data[5] = 6;
  commandI2CData.size = 6;
  i2c_inout(commandI2CData.data, commandI2CData.size, commandI2CData.reciveBuf, &timer1RefreshTimeCallBack);
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
  cli();
  TCNT1 += 3036; // Корректировка времени срабатывания преполнения к одной секунде.
  timer1IncrementCurrentTime();
  ledSw;
  //uart_writelnHEX(zs042_seconds);
  switch (zs042_seconds) {
    case 2 : 
      queue_putTask(DO_REFRESH_TIME);
      break;
    case 3 : // Вставляем задачу поиска будильников
      queue_putTask2b(DO_FETCH_DAILY_ALARM, 0, AT24C32_ALARMS_BLOCK_MAX_REC_COUNT);
      break;  
  }
  _log(0xABCD);
  sei();
}

void timer_init() {
  // Разрешить светодиод arduino pro mini.
  DDRB |= _BV(DDB5);
  // Делитель счетчика 256 (CS10=0, CS11=0, CS12=1).
  // 256 * 65536 = 16 777 216 (тактов)
  TCCR1B |= _BV(CS12);
  //TCCR1B |= _BV(CS10); //Включить для мигания  4 -ре секунды
  // Включить обработчик прерывания переполнения счетчика таймера.
  TIMSK1 = _BV(TOIE1);
  // PRR &= ~(_BV(PRTIM1));
  TCNT1 += 3036;
}
/*
 * Уход за цветами
 * 
 * Принимает через USART команды I2C шины в формате:
 *   IS1A1D1D2D3...S2A2D4D5D6...
 * где:
 *    I              - префикс
 *    S1, S2,...,Sn - Размер текущего блока;
 *    A1, A1,...,An - Адрес на шине I2C с флагом чтение/запись;
 *    D1, D2,...,Dn - Отправляемые данные, или ожидаемый объем принимаемых данных.
 *  Например I02D00002D107 - отправить байт 00 на устройство D0, прочитать 7-мь байт 
 *  с устройства D1.
 */

#include <avr/io.h>
#include <avr/interrupt.h>
#include <avr/sleep.h>
#define F_CPU 16000000
#include <util/delay.h>
#include "avr-gardener.h"

//Настройки магающей лампочки
#define ledOn PORTB |= _BV(PINB5)  
#define ledOff PORTB &= ~(_BV(PINB5))
#define ledSw PORTB ^= _BV(PINB5)

#include "tools/uart_async.h"
#include "tools/i2c_async.h"
#include "tools/queue_tasks.h"

byte decToBcd(byte val){
  return ( (val/10*16) + (val%10) );
}

byte bcdToDec(byte val){
  return ( (val/16*10) + (val%16) );
}

unsigned char hexToCharOne(char c) {
  switch(c) {
    case '0' : return 0x00;
    case '1' : return 0x01;
    case '2' : return 0x02;
    case '3' : return 0x03;
    case '4' : return 0x04;
    case '5' : return 0x05;
    case '6' : return 0x06;
    case '7' : return 0x07;
    case '8' : return 0x08;
    case '9' : return 0x09;
    case 'A' : return 0x0A;
    case 'B' : return 0x0B;
    case 'C' : return 0x0C;
    case 'D' : return 0x0D;
    case 'E' : return 0x0E;
    case 'F' : return 0x0F;
    default : return 0xFF;
  }
}

/**
 * @brief Преобразует строку HEX символов (0-F) в массив байт.
 * @param str  строка HEX символов
 * @param result  указатель на буфер для сохранения результата.
 * @return возвращает длинну полученного массива.
 */
byte parse_HEX_string(char* str, byte* result) {
  byte pos = 0;
  byte tmp;
  while((str[pos*2] != 0) && 
          (pos < HEX_CMD_MAX_SIZE)) {
      result[pos] = hexToCharOne(str[pos*2]);
      tmp = hexToCharOne(str[pos*2+1]);
      if ((tmp & 0xF0) || (result[pos] & 0xF0)) {
        return 0;
      }
      result[pos] <<= 4;
      result[pos] |= tmp;
      pos++;
    }
  return pos;
}

/**
 * @brief Разбирает строковый сценарий и отправляет его на I2C шину
 * @param str Сценрий в ввиде IS1A1D1D2D3...S2A2D4D5D6...
 *    I              - префикс
 *    S1, S2,...,Sn - Размер текущего блока;
 *    A1, A1,...,An - Адрес на шине I2C с флагом чтение/запись;
 *    D1, D2,...,Dn - Отправляемые данные, или ожидаемый объем принимаемых данных.
 *  Например I02D00002D107 - отправить байт 00 на устройство D0, прочитать 7-мь байт 
 *  с устройства D1.
 */
void commands_reciver(char* str) {
  byte pos = 0;
  byte tmp;
  byte cmd[3];
  if (str[0] == 'W') { // Спец задания
    pos = parse_HEX_string(&str[1], cmd);
    if (! pos) {
      uart_writeln("PARS ERR");
      return;
    }
    uart_writelnHEXEx(cmd, 3);
    switch (cmd[0]) {
      case DO_COMMAND_CLEAN_24C32N :
        uart_writeln("E0");
        queue_putTask2b(cmd[0], 0, 0);
        break;
      default : queue_putTask2b(cmd[0], cmd[1], cmd[2]);
    }
  } else {
    if (str[0] == 'I') {
      commandI2CData.reciveBufSize = 0;
      pos = parse_HEX_string(str + 1, commandI2CData.data);
      if (! pos) {
        uart_writeln("PARS ERR");
        return;
      }
      tmp = 1;
      while(tmp < pos) {
        if (commandI2CData.data[tmp] & 1) {// Если I2C адрес для чтения
          commandI2CData.reciveBufSize += commandI2CData.data[tmp+1];
        }
        tmp += commandI2CData.data[tmp-1] + 1;
      }
      commandI2CData.size = pos; // Размер буфера
      queue_putTask(DO_COMMAND_INOUT_I2C);
    } else uart_writeln("Unknown command.");
  }
}

ISR (INT0_vect) {

}

// Прерывание переполнения таймера
ISR (TIMER1_OVF_vect) {
  static byte timer1_position = 0; // Переключатель задач таймера
  cli();
  TCNT1 += 3036; // Корректировка времени срабатывания преполнения к одной секунде.
  ledSw;
  switch (timer1_position) {
    case 0 : 
      commands_reciver("I02D00002D107");
      break;
  }
  timer1_position++;
  if (timer1_position > 59) timer1_position = 0;
  sei();
}

ISR (BADISR_vect) {
  cli();
  TWSR = 0;
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

void callBackForI2CCommand(unsigned char result) {
  switch(result) {
    case TW_MT_DATA_ACK : 
      uart_writeln("OK");
      break;
    case TW_MR_DATA_NACK : //Результат получен
      uart_writelnHEXEx(commandI2CData.reciveBuf, commandI2CData.reciveBufSize);
      break;
    default : 
      uart_write("ERROR ");
      uart_writelnHEX(result);
  }
}

void int0_init(void) {
  PORTD |= _BV(PD2); // подключаем pull-up резистор
  EIMSK |= _BV(INT0); // Активируем прерывание
  // Срабатывание по краю падения уровня
  EICRA |= _BV(ISC01);
  EICRA &= ~(_BV(ISC00));
}

void eeprom_24C32N_clean_callBack(byte result) {
  uint16_t addr = (i2c_result[0] * 256) + i2c_result[1];
  if (addr > 4090) {
    uart_write("Stop at "); uart_writelnHEXEx(i2c_result, 2);
    return;
  }
    switch(result) {
    case TW_MT_DATA_ACK : 
      uart_writeln("OK");
      break;
    default : 
      uart_write("ERROR ");
      uart_writelnHEX(result);
  }
  _delay_ms(1);
  while(i2c_state != I2C_STATE_FREE) sleep_mode();
  eeprom_24C32N_clean(i2c_result);
}

void eeprom_24C32N_clean(byte* adr) {
  //Очистить 8 байт
  //uart_write("Start at "); uart_writelnHEXEx(adr, 2);
  commandI2CData.data[0] = 11; // Адрес устройства + 2 -байта номер ячейки + 8 байт нули = 11 
  commandI2CData.data[1] = AT24C32_ADDRESS;
  commandI2CData.data[2] = adr[0];
  commandI2CData.data[3] = adr[1];
  for(commandI2CData.size = 4; commandI2CData.size < (4 + 8); commandI2CData.size++ ) commandI2CData.data[commandI2CData.size] = 0;
  adr[1] += 8;
  if (adr[1] < 8) adr[0]++;
  i2c_inout(commandI2CData.data, commandI2CData.size, adr, &eeprom_24C32N_clean_callBack);
}

int main(void) {
  // Разрешить светодиод arduino pro mini.
  DDRB |= _BV(DDB5);
  timer_init(); // Переполнение таймера примерно 1 раз в секунду
  uart_async_init(); // Прерывания для ввода/вывода через USART
  i2c_init();
  queue_init();  // Очередь диспетчера задач (главный цикл)
  //int0_init(); // Прерывание INT0 по спадающей границе. Для RTC ZA-042.
  sei();
  uart_readln(&commands_reciver);
#ifdef _DEBUG
  uart_writeln("start");
#endif
  // Бесконечный цикл с энергосбережением.
  for(;;) {
    switch(queue_getTask()) {
      case DO_COMMAND_INOUT_I2C : // Чтение данных из I2C и вывод в USART
        i2c_inout(commandI2CData.data, commandI2CData.size, commandI2CData.reciveBuf, &callBackForI2CCommand);
        break;
      case DO_COMMAND_CLEAN_24C32N : // Очистка eeprom
        eeprom_24C32N_clean(queue_tasks_current.data);
      default : sleep_mode();
    }
  }
  return 0;
}

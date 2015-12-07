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
#include <util/crc16.h>
#include "avr-gardener.h"

#include "tools/uart_async.h"
#include "tools/i2c_async.h"
#include "tools/queue_tasks.h"
#include "tools/timer16.h"
#include "tools/zs042.h"

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

ISR (BADISR_vect) {
  cli();
  TWSR = 0;
  sei();
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
  //_delay_ms(1);
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

void relay_touch(byte block, byte touching, byte values) {
  uart_writeln("==");
  uart_writelnHEX(touching);
  uart_writelnHEX(values);
  if (block == 1) {
    byte p;
    DDRB |= 0b00000011;
    for(p = 1; p < 9 ; p <<= 1) {
      if (p & touching) {
        switch(p) {
          case 1 : 
            if (values & p) PORTB &= ~(_BV(PORTB0));
            else PORTB |= _BV(PORTB0);
            break;
          case 2 :
            if (values & p) PORTB &= ~(_BV(PORTB1));
            else PORTB |= _BV(PORTB1);
            break;
          case 4 :
            break;
          case 8 : 
            break; 
        }
      }
    }
  }
}

struct rec_alarm alarm;

void callBackForLoadAlarmShow(unsigned char result) {
  switch(result) {
    case TW_MR_DATA_NACK : //Результат получен
      uart_writelnHEX(alarm.alarm_time.minut);
      uart_writelnHEX(alarm.alarm_time.hour);
      uart_writelnHEX(alarm.alarm_time.dayOfMonth);
      uart_writelnHEX(alarm.flags.b);
      uart_writelnHEX(alarm.task.task_id);
      if (alarm.flags.f.reserved1) uart_write("1"); else uart_write("0");
      if (alarm.flags.f.minut) uart_write("1"); else uart_write("0");
      if (alarm.flags.f.hour) uart_write("1"); else uart_write("0");
      if (alarm.flags.f.dayOfMonth) uart_write("1"); else uart_write("0");
      if (alarm.flags.f.month) uart_write("1"); else uart_write("0");
      if (alarm.flags.f.year) uart_write("1"); else uart_write("0");
      if (alarm.flags.f.dayOfWeek) uart_write("1"); else uart_write("0");
      if (alarm.flags.f.enable) uart_writeln("1"); else uart_writeln("0");
      break;
    default : 
      uart_write("ERROR ");
      uart_writelnHEX(result);
  }
}

void callBackForLoadAlarm(unsigned char result) {
  switch(result) {
    case TW_MR_DATA_NACK : //Результат получен
      if ( get_crc16((byte *) &alarm, 14) != alarm.crc16 ) {
        uart_write("CRC ERROR ");
        uart_writelnHEX(alarm.num);
      }
      if ((alarm.flags.b & 0b11111100) == 0b10000100) { // Если ежедневный будильник
        if ((alarm.alarm_time.minut == current_time.minut) && 
            (alarm.alarm_time.hour == current_time.hour)) {
          uart_writelnHEX(alarm.alarm_time.minut);
          uart_writelnHEX(alarm.task.task_id);
          uart_writelnHEX(alarm.task.data[0]);
          uart_writelnHEX(alarm.task.data[1]);
          queue_putTask2b(alarm.task.task_id, alarm.task.data[0], alarm.task.data[1]);
        }
      }
      break;
    default : 
      uart_write("ERROR ");
      uart_writelnHEX(result);
  }
}

byte zs042LoadAlarm(byte n, byte show) {
  if (i2c_state) return i2c_state;
  // Установка адреса
  commandI2CData.data[0] = 03; // Размер блока установки адреса
  commandI2CData.data[1] = AT24C32_ADDRESS; // Адресс для записи
  // Вычисляем адрес будильника 
  uint16_t addr = AT24C32_ALARMS_BLOCK_START + AT24C32_ALARMS_BLOCK_BYTES_BY_RECORD * n;
  commandI2CData.data[2] = addr >> 8;
  commandI2CData.data[3] = addr & 0x00FF;
  // Блок чтения с I2C
  commandI2CData.data[4] = 02; // Размер блока чтения
  commandI2CData.data[5] = AT24C32_ADDRESS + 1; // Адресс для чтения
  commandI2CData.data[6] = 16;
  commandI2CData.size = 7;
  alarm.num = n;
  //uart_writelnHEXEx(commandI2CData.data, 7);
  if (show) return i2c_inout(commandI2CData.data, commandI2CData.size, (byte*) &alarm, &callBackForLoadAlarmShow);
  else return i2c_inout(commandI2CData.data, commandI2CData.size, (byte*) &alarm, &callBackForLoadAlarm);
}

void start(void) {
  timer_init(); // Переполнение таймера примерно 1 раз в секунду
  uart_async_init(); // Прерывания для ввода/вывода через USART
  uart_readln(&commands_reciver); // Процедура принимает построчный ввод команд с UASART
  i2c_init(); // Прерывание I2C Шины
  queue_init();  // Очередь диспетчера задач (главный цикл)
  //int0_init(); // Прерывание INT0 по спадающей границе. Для RTC ZA-042.
  zs042_init_time(&current_time);  // Запрос времени без преррываний
}

void doFetchDailyAlarm(byte first, byte last) {
  if ( zs042LoadAlarm(first, 0) == I2C_STATE_FREE ) first++;
  if (first < last)  queue_putTask2b(DO_FETCH_DAILY_ALARM ,first, last);
}

void crc16test(byte b1, byte b2) {
  uint16_t r = 0;
  r = _crc16_update(r, b1);
  r = _crc16_update(r, b2);
  uart_writelnHEXEx((byte*) &r, 2);
}

int main(void) {
  start();
  sei();
//#ifdef _DEBUG
  uart_writeln("start");
//#endif
  // Бесконечный цикл с энергосбережением.
  for(;;) {
    switch(queue_getTask()) {
      case DO_REFRESH_TIME :
        timer1RefreshTime(); 
        break; 
      case DO_FETCH_DAILY_ALARM :
        doFetchDailyAlarm(queue_tasks_current.data[0], queue_tasks_current.data[1]);
        break;
      case DO_COMMAND_INOUT_I2C : // Чтение данных из I2C и вывод в USART
        while ( i2c_inout(commandI2CData.data, 
                          commandI2CData.size, 
                          commandI2CData.reciveBuf, 
                          &callBackForI2CCommand) != I2C_STATE_FREE
              ) sleep_mode();
        break;
      case DO_COMMAND_CLEAN_24C32N : // Очистка eeprom
        eeprom_24C32N_clean(queue_tasks_current.data);
        break;
      case DO_TOUCH_RELAY_A : // Включение/выключение блока реле.
        relay_touch(1, queue_tasks_current.data[0], queue_tasks_current.data[1]);
        break;
      case DO_LOAD_ALARM :
        zs042LoadAlarm(queue_tasks_current.data[0], 1);
        break;
      case DO_CRC16_TEST :
        crc16test(queue_tasks_current.data[0], queue_tasks_current.data[1]);
        break;
      default : sleep_mode();
    }
  }
  return 0;
}

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
#include "tools/error.h"

unsigned char hexToCharOne(char c) {
  if ((c > 47) && (c<58)) return c-48;
  switch(c) {
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
      _log(ERR_COM_PARSER_PARS_ERR);
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
      cli();
      if (i2c_state || commandI2CData.task) {
        _log(ERR_I2C_BUSY);
        return;
      }
      commandI2CData.reciveBufSize = 0;
      pos = parse_HEX_string(str + 1, commandI2CData.data);
      if (! pos) {
        _log(ERR_COM_PARSER_PARS_ERR);
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
    } else _log(ERR_COM_PARSER_UNKNOW_COM);
  }
}

ISR (BADISR_vect) {
  cli();
  TWSR = 0;
  sei();
}

byte callBackForI2CCommand(unsigned char result) {
  switch(result) {
    case TW_MT_DATA_ACK : 
      uart_writeln("OK");
      break;
    case TW_MR_DATA_NACK : //Результат получен
      uart_writelnHEXEx(commandI2CData.reciveBuf, commandI2CData.reciveBufSize);
      break;
    default : 
      _log(ERR_I2C(result));
  }
  return 0;
}

void int0_init(void) {
  PORTD |= _BV(PD2); // подключаем pull-up резистор
  EIMSK |= _BV(INT0); // Активируем прерывание
  // Срабатывание по краю падения уровня
  EICRA |= _BV(ISC01);
  EICRA &= ~(_BV(ISC00));
}

/**
 * @brief Обратный вызов для процедуры отчиски eeprom - 8 - байт очищено.
 * Увеличивает текущий адрес на 8 и ставит в очередь задание на отчистку 
 * следующих 8-ми байт.
 * @param result статус I2C шины.
 * @return Всегда 0.
 */

byte eeprom_24C32N_clean_callBack(byte result) {
  if ((i2c_result[0] > 0x0E) && (i2c_result[1] > 0xFA)) /* (addr > 4090) */{
    uart_write("Stop at "); uart_writelnHEXEx(i2c_result, 2);
    return 0;
  }
  switch(result) {
    case TW_MT_DATA_ACK : 
      uart_writeln("OK");
      break;
    default : 
      _log(ERR_I2C(result));
  }
  i2c_result[1] += 8;
  if (i2c_result[1] < 8) i2c_result[0]++;
  queue_putTask2b(DO_COMMAND_CLEAN_24C32N, i2c_result[0], i2c_result[1]);
  return 0;
}

/**
 * @brief Инициирует обнуление 8-ми байт в EEPROM (AE).
 * @param adr Адрес перврй ячейки eeprom - по завершению 
 * будет увеличен на 8.
 */
void eeprom_24C32N_clean(byte* adr) {
  commandI2CData.data[0] = 11; // Длина скрипта
  commandI2CData.data[1] = AT24C32_ADDRESS;
  commandI2CData.data[2] = adr[0];
  commandI2CData.data[3] = adr[1];
  for(commandI2CData.size = 4; commandI2CData.size < (4 + 8); commandI2CData.size++ ) 
    commandI2CData.data[commandI2CData.size] = 0;
  adr[1] += 8;
  if (adr[1] < 8) adr[0]++;
  i2c_inout(commandI2CData.data, commandI2CData.size, adr, &eeprom_24C32N_clean_callBack);
}

void relay_init(void) {
  DDRB  |= 0b00000011;
  PORTB |= 0b00000011;
}

void relay_touch(byte block, byte touching, byte values) {
  if (block == 1) {
    byte p;
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

byte callBackForLoadAlarmShow(unsigned char result) {
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
      _log(ERR_I2C(result));
  }
  return 0;
}

/**
 * @brief Получает результат загрузки будильника с I2C шины. 
 *    В случае если время пришло - ставит задачу в очередб на 
 *    исполнение
 * @param result
 * @return В случае не совпадения контрольной сумма вернет не 0
 *         (для повторения процесса чтения)
 */
byte executeLoadedAlarm(unsigned char result) {
  switch(result) {
    case TW_MR_DATA_NACK : //Результат получен
      if ( get_crc16((byte *) &alarm, 14) != alarm.crc16 ) {
        _log(ERR_ALARM_CRC(alarm.num));
        return 1;
      }
      if ((alarm.flags.b & 0b11111100) == 0b10000100) { // Если ежедневный будильник
        if ((alarm.alarm_time.minut == current_time.minut) && 
            (alarm.alarm_time.hour == current_time.hour)) {
          queue_putTask2b(alarm.task.task_id, alarm.task.data[0], alarm.task.data[1]);
        }
      }
      break;
    default : 
      _log(ERR_I2C(result));
  }
  return 0;
}

/**
 * @brief Загружаем будильк и что-то с ним делаем
 * @param n Номер загружаемого будильника
 * @param do_some Код операции
 * @return Возвращает статус шины I2C до запуска.
 *  Если не 0 значит шина занята и задача не выполнена.
 */
byte zs042LoadAlarm(byte n, byte do_some) {
  cli();
  if (i2c_state || commandI2CData.task) {
    sei();
    return 1;
  }
  commandI2CData.task = DO_FETCH_DAILY_ALARM;
  sei();
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
  switch (do_some) {
    case ALARM_EXEC : 
      i2c_inout(commandI2CData.data, 
                commandI2CData.size, 
                (byte*) &alarm, 
                &executeLoadedAlarm);
      break;
    case ALARM_SHOW : 
      i2c_inout(commandI2CData.data, 
                commandI2CData.size, 
                (byte*) &alarm, 
                &callBackForLoadAlarmShow);
      break;
  }
  commandI2CData.task = 0;
  return I2C_STATE_FREE;
}

void start(void) {
  timer_init(); // Переполнение таймера примерно 1 раз в секунду
  uart_async_init(); // Прерывания для ввода/вывода через USART
  uart_readln(&commands_reciver); // Процедура принимает построчный ввод команд с UASART
  i2c_init(1); // Прерывание I2C Шины
  queue_init();  // Очередь диспетчера задач (главный цикл)
  zs042_init_time(&current_time);  // Запрос времени без преррываний
  relay_init();
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
        // Если шина не занята переходим к следующему будильнику
        queue_tasks_current.data[0] += 
            (zs042LoadAlarm(queue_tasks_current.data[0], ALARM_EXEC) ? 0 : 1);
        // Если будильник был последним, то не переходим к следующему будильнику
        if (queue_tasks_current.data[0] < queue_tasks_current.data[1])
            queue_putTask2b(DO_FETCH_DAILY_ALARM, 
                          queue_tasks_current.data[0], 
                          queue_tasks_current.data[1]);
        break;
      case DO_COMMAND_INOUT_I2C : // Чтение данных из I2C и вывод в USART
        while ( i2c_inout(commandI2CData.data, 
                          commandI2CData.size, 
                          commandI2CData.reciveBuf, 
                          &callBackForI2CCommand) != I2C_STATE_FREE
              ) sleep_mode();
        break;
      case DO_COMMAND_CLEAN_24C32N : // Очистка eeprom
        cli(); // Занимаем буфер команд I2C
        if (i2c_state || commandI2CData.task) { // Если шина занята откладываем задачу
          queue_putTask2b(DO_COMMAND_CLEAN_24C32N, queue_tasks_current.data[0], queue_tasks_current.data[1]);
          sei();
          break;
        }
        commandI2CData.task = DO_COMMAND_CLEAN_24C32N; // Заняли буфер
        cli();
        commandI2CData.data[0] = 11; // Длина скрипта
        commandI2CData.data[1] = AT24C32_ADDRESS;
        commandI2CData.data[2] = queue_tasks_current.data[0];
        commandI2CData.data[3] = queue_tasks_current.data[1];
        for(commandI2CData.size = 4; commandI2CData.size < (4 + 8); commandI2CData.size++ ) 
          commandI2CData.data[commandI2CData.size] = 0;
        commandI2CData.reciveBuf[0] = queue_tasks_current.data[0];
        commandI2CData.reciveBuf[1] = queue_tasks_current.data[1];
        i2c_inout(commandI2CData.data, 
                  commandI2CData.size, 
                  commandI2CData.reciveBuf, 
                  &eeprom_24C32N_clean_callBack);
        commandI2CData.task = 0; // Заняли шину - буфер можно отметить как свободный
        break;
      case DO_TOUCH_RELAY_A : // Включение/выключение блока реле.
        relay_touch(1, queue_tasks_current.data[0], queue_tasks_current.data[1]);
        uart_write("touch ");
        uart_writelnHEXEx(queue_tasks_current.data, 2);
        break;
      case DO_LOAD_ALARM :
        zs042LoadAlarm(queue_tasks_current.data[0], ALARM_SHOW);
        break;
      case DO_PRINT_TIME :
        _uart_writeHEX(decToBcd(current_time.hour));
        uart_write(":");
        _uart_writeHEX(decToBcd(current_time.minut));
        uart_writeln("");
        break;
      case DO_COMMAND_SET_SPEED :
        timer16_start_value = (queue_tasks_current.data[0] << 8) + queue_tasks_current.data[1];
      default : sleep_mode();
    }
  }
  return 0;
}

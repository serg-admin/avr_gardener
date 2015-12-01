/*
 * Уход за цветами
 * 
 * Принимает через USART команды I2C шины в формате 
 *   IXXXXXXXX - где I префикс
 *                   XX - байты данных в шеснадцатиричном виде
 */

#include <avr/io.h>
#include <avr/interrupt.h>
#include <avr/sleep.h>
#define byte unsigned char

//Настройки магающей лампочки
#define ledOn PORTB |= _BV(PINB5)  
#define ledOff PORTB &= ~(_BV(PINB5))
#define ledSw PORTB ^= _BV(PINB5)

#define DS3231_ADDRESS 0xD0 // Модуль реального времени.
#define AT24C32_ADDRESS 0xAE // eeprom 4 килобайта.
#define I2C_BUF_SIZE 16 // Размер буфер для отладки и приема данных через I2C шину
unsigned char ds3231_buf[I2C_BUF_SIZE]; // Буфер для отладки и приема данных через I2C шину

#define HEX_CMD_MAX_SIZE 16 // Длина шеснадцатиричной команды приходящей с USART (без учета префикса)


#include "tools/i2c_async.h"
#include "tools/uart_async.h"
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

byte command_I2C_buf[HEX_CMD_MAX_SIZE];
struct str_commandI2C {
  byte device_id;
  byte* data;
  byte size;
} commandI2CData;

// Идентификаторы задач для главного цикла
#define COMMAND_SEND_I2C 0x30
#define COMMAND_RECIVE_I2C 0x31

void commands_reciver(char* str) {
  byte pos = 0;
  byte tmp;
  if (str[0] == 'I') {
    while((str[pos*2+1] != 0) && 
          (pos < (HEX_CMD_MAX_SIZE-2/*два байта для вычисляемых параметров*/))) {
      command_I2C_buf[pos] = hexToCharOne(str[pos*2+1]);
      tmp = hexToCharOne(str[pos*2+2]);
      if ((tmp > 0xF) || (command_I2C_buf[pos] > 0xF)) {
        uart_writeln("PARS ERR");
        return;
      }
      command_I2C_buf[pos] <<= 4;
      command_I2C_buf[pos] |= tmp;
      pos++;
    }
    commandI2CData.device_id = command_I2C_buf[0]; //Адрес устройства
    commandI2CData.data = (command_I2C_buf+1); //Массив для отправки
    commandI2CData.size = pos - 1; //Количество отправляемых байт
    if (command_I2C_buf[0] & 1) {
      commandI2CData.size = command_I2C_buf[1];
      queue_putTask(COMMAND_RECIVE_I2C);
    } else queue_putTask(COMMAND_SEND_I2C);
  } else uart_writeln("Unknown command.");
}

byte diff[2];

ISR (INT0_vect) {
  //uart_writeln("INT0");
  diff[0] = TCNT1H;
  diff[1] = TCNT1L;
}

// Прерывание переполнения таймера
ISR (TIMER1_OVF_vect) {
  static byte timer1_position = 0; // Переключатель задач таймера
  cli();
  TCNT1 += 3036; // Корректировка времени срабатывания преполнения к одной секунде.
  ledSw;
  switch (timer1_position) {
    case 0 : 
      uart_writeln("T1 POS = 0");
      commands_reciver("ID000");
      break;
    case 1 : 
      uart_writeln("T1 POS = 1");
      commands_reciver("ID105");
      uart_writelnHEXEx(diff, 2);
      break;
    case 60 : 
      commands_reciver("ID000");
      break;
    case 61 : 
      commands_reciver("ID105");
      uart_writelnHEXEx(diff, 2);
      break;
    case 120 : 
      commands_reciver("ID000");
      break;
    case 121 : 
      commands_reciver("ID105");
      uart_writelnHEXEx(diff, 2);
      break;    
    case 180 : 
      commands_reciver("ID000");
      break;
    case 181 : 
      commands_reciver("ID105");
      uart_writelnHEXEx(diff, 2);
      break;
    case 240 : 
      commands_reciver("ID000");
      break;
    case 241 : 
      commands_reciver("ID105");
      break;      
  }
  /*if (timer1_doing == 0) {
    queue_putTask(DO_TIMER1_OVF);
    timer1_doing = 1;
  }*/
  timer1_position++;
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
      uart_writelnHEXEx(commandI2CData.data, commandI2CData.size);
      break;
    default : 
      uart_write("ERROR ");
      uart_writelnHEX(result);
  }
}

void init_int0(void) {
  PORTD |= _BV(PD2); // подключаем pull-up резистор
  EIMSK |= _BV(INT0); // Активируем прерывание
  // Срабатывание по краю падения уровня
  EICRA |= _BV(ISC01);
  EICRA &= ~(_BV(ISC00));
}

int main(void) {
  // Разрешить светодиод arduino pro mini.
  DDRB |= _BV(DDB5);
  timer_init(); // Переполнение таймера примерно 1 раз в секунду
  uart_async_init(); // Прерывания для ввода/вывода через USART
  i2c_init();
  queue_init();  // Очередь диспетчера задач (главный цикл)
  init_int0(); // Прерывание INT0 по спадающей границе. Для таймера
  sei(); // Разрешить прерывания.
  uart_readln(&commands_reciver);
  uart_writeln("start");
  // Бесконечный цикл с энергосбережением.
  for(;;) {
    switch(queue_getTask()) {
      /*case DO_TIMER1_OVF :
        i2c_init();  // Тупой сброс I2C шины. todo - сделать тест на залипание шины.
        break;*/
      case COMMAND_SEND_I2C : // Отправка данных из USART команды в I2C
        i2c_send(commandI2CData.device_id, // Адресс.
                 commandI2CData.data, // Буфер.
                 commandI2CData.size , // Количество.
                 &callBackForI2CCommand);
        break;
      case COMMAND_RECIVE_I2C : // Чтение данных из I2C и вывод в USART
        i2c_recive(commandI2CData.device_id, // Адресс.
                 commandI2CData.data, // Буфер.
                 commandI2CData.size, // Количество.
                 &callBackForI2CCommand);
        break;
      default : sleep_mode();
    }
  }
  return 0;
}

#include <avr/io.h>
#include <util/twi.h>
#include <avr/interrupt.h>
#include "uart_async.h"
#include "common.h"
#include "i2c_async.h"
#include "timer16.h"

char i2c_dev_addr;
unsigned char i2c_result_pos;
unsigned char i2c_result_end_pos;
unsigned char* i2c_buf;
unsigned char i2c_buf_pos;
unsigned char i2c_size; // Размер полученного буфера
unsigned char i2c_end_of_block; // Позиция окончания текуего блока
unsigned char i2c_last_device_id;
void (*i2c_callback)(unsigned char);

void set_free(byte* data) {
  i2c_state = I2C_STATE_FREE;
  uart_writeln("free");
}

/**
 * @brief Устанавливает первоначальное состояние переменных:
 *
 */
void i2c_init(void) {
  // Настраеваем прерывание ножки SDA, для ожидания установки состояния STOP.
  DDRC  &= ~(_BV(DDC4));
  DDRC  &= ~(_BV(DDC5));
  PORTC |= _BV(PC4) | _BV(PC5);
  //PCICR |= _BV(PCIE1);
  //PCMSK1 &= ~(_BV(PCINT12));
  //PCMSK1 &= ~(_BV(PCINT13));
  
  TWBR = 0x05; // Делитель = TWBR * 2.
  TWCR = 0; // Включить прерывание.
  i2c_state = I2C_STATE_FREE;
}

/**
 * @brief Осущетвляет отправку/прием/рестрат I2C шины в соответсвии 
 *        со сценарием содержащимся в буфере.
 * @param script Последовательность блоков команд в формате:
 *        - размер блока (x00 - xFF);
 *        - I2C адрес устройства (x00 - xFF);
 *        - блок данных для передачи/длинна принимаемых данных.
 * @param size Размер сценария в байтах.
 * @param result Буфер для хранения результатов операций чтения.
 * @param callback Указатель на функцию которая будет вызвана после 
 *        выполнения сценария. Функция получает статус завершения.
 * @return При успешном запуске вернет I2C_STATE_FREE, в противном
 *         случае код процесса, которым занята шина.
 */
unsigned char i2c_inout(unsigned char* script,
                        unsigned char size,
                        unsigned char* result, 
                        void (*callback)(unsigned char)) {
  if (i2c_state) return i2c_state;
  i2c_state = I2C_STATE_INOUT;
  i2c_buf = script;
  i2c_buf_pos = 0;
  i2c_size = size;
  i2c_result = result;
  i2c_result_end_pos = 0;
  i2c_result_pos = 0;
  i2c_end_of_block = script[i2c_buf_pos++] + 1;
  i2c_callback = callback;
  TWCR = _BV(TWINT) | _BV(TWSTA) | _BV(TWEN) | _BV(TWIE); // Send START condition.
  return I2C_STATE_FREE;
}

void i2c_stop(unsigned char state) {
  TWCR = _BV(TWINT) | _BV(TWEN) | _BV(TWSTO); // Завершение передача
  //SDA_WAIT_LEVEL;
  i2c_state = I2C_STATE_STOPPING;
  sei();
  timer1PutTask(30000, &set_free, 0); // Задержка примерно 1/62500 * 30 секунды
  uart_writelnHEX(TIMSK1);
  //set_free(0);
  if (i2c_callback != 0) i2c_callback(state);
}

void i2c_init_next_block(void) {
  i2c_end_of_block += i2c_buf[i2c_buf_pos++] + 1;
  i2c_last_device_id = i2c_buf[i2c_buf_pos++];
  TWDR = i2c_last_device_id;
  if (i2c_last_device_id & 1) {// Если чтение с шины, вычисляем количество читаемых байт.
    i2c_result_end_pos += i2c_buf[i2c_buf_pos++];
  }
  TWCR = _BV(TWINT) | _BV(TWSTA) | _BV(TWEN) | _BV(TWIE);
}

void i2c_inout_isp(unsigned char state) {
  switch(state) {
    case TW_START : //Шина I2C переведена в состояние start transmission. Запрашиваем устройство.
      i2c_last_device_id = i2c_buf[i2c_buf_pos++];
      TWDR = i2c_last_device_id;
      if (TWDR & 1) {// Если чтение с шины
        i2c_result_end_pos += i2c_buf[i2c_buf_pos++];
      }
      TWCR = _BV(TWINT) | _BV(TWEN) | _BV(TWIE); // Load SLA+W into TWDR Register. Clear TWINT bit in TWCR to start transmission of address.
      break;
    case TW_MT_SLA_ACK : // Устройство ответело, ожидает данные.
      TWDR = i2c_buf[i2c_buf_pos++];
      TWCR = _BV(TWINT) | _BV(TWEN) | _BV(TWIE);
      break;
    case TW_MT_DATA_ACK : // Данные успешно переданы.
      //uart_writeln("TW_MT_DATA_ACK= ");
      if (i2c_buf_pos < i2c_end_of_block) { // Если не все данные переданы - продолжаем передачу
        TWDR = i2c_buf[i2c_buf_pos++];
        TWCR = _BV(TWINT) | _BV(TWEN) | _BV(TWIE);
      } else { 
        if (i2c_buf_pos < i2c_size)// Инициализируем следующий блок сценария.
          i2c_init_next_block();
        else i2c_stop(state);
      }
      break;
    case TW_MT_SLA_NACK : 
      if (i2c_buf_pos < i2c_end_of_block) { // Если в буфере есть данные - продолжаем передачу
        TWDR = i2c_buf[i2c_buf_pos++];
        TWCR = _BV(TWINT) | _BV(TWEN) | _BV(TWIE);
      } else {
        if (i2c_buf_pos < i2c_size)// Инициализируем следующий блок сценария.
          i2c_init_next_block();
        else i2c_stop(state);
      }
      break;
    case TW_REP_START : //Шина I2C перестартавала. Запрашиваем устройство.
      TWCR = _BV(TWINT) | _BV(TWEN) | _BV(TWIE);
      break;
    case TW_MR_SLA_ACK : // Устройство ответело, готово слать данные.
      TWCR = _BV(TWINT) | _BV(TWEN) | _BV(TWIE);
      break;
    case TW_MR_DATA_ACK : // Устройство ответело, пришел байт данных.
      i2c_result[i2c_result_pos++] = TWDR;
      if (i2c_result_pos < i2c_result_end_pos) {
        TWCR = _BV(TWINT) | _BV(TWEN) | _BV(TWIE);
      } else {
        if (i2c_buf_pos < i2c_size)// Инициализируем следующий блок сценария.
          i2c_init_next_block();
        else i2c_stop(state);
      }
      break;
    case TW_MR_DATA_NACK : // Устройство ответело, пришел байт данных.
      i2c_result[i2c_result_pos++] = TWDR;
      if (i2c_result_pos < i2c_result_end_pos) {
        TWDR = i2c_last_device_id;
        TWCR = _BV(TWINT) | _BV(TWSTA) | _BV(TWEN) | _BV(TWIE); // Рестарт для следующего байта
      } else i2c_stop(state);
      break;
    default : i2c_stop(state);
  }
}

// Обработка событий I2C
ISR (TWI_vect) {
  cli();
  if (I2C_STATE_INOUT) i2c_inout_isp(TWSR & 0xF8);
  else {
    TWCR = 0; // Завершение передача
    i2c_state = I2C_STATE_FREE;
  }
  sei();
}

// Ожидание установки уровня SDA при остановке.
ISR (PCINT1_vect) {
/*  cli();
  PCMSK1 &= ~(_BV(PCINT12));
  if (i2c_state == I2C_STATE_STOPPING) i2c_state = I2C_STATE_FREE;
  sei();*/
}
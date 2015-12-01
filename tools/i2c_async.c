#include <avr/io.h>
#include <util/twi.h>
#include <avr/interrupt.h>
#include "i2c_async.h"

unsigned char i2c_state;
char i2c_dev_addr;
unsigned char* i2c_buf;
unsigned char i2c_buf_pos;
unsigned char i2c_size;
void (*i2c_callback)(unsigned char);

void i2c_init(void) {
  TWBR = 0xFF; //Делитель = TWBR * 2.
  TWCR = 0; //Включить прерывание.
  i2c_state = I2C_STATE_FREE;
}

unsigned char i2c_send(char addr, unsigned char* buf, unsigned char size, void (*callback)(unsigned char)) {
  if (i2c_state) return i2c_state;
  i2c_dev_addr = addr;
  i2c_buf = buf;
  i2c_buf_pos = 0;
  i2c_size = size;
  i2c_callback = callback;
  i2c_state = I2C_STATE_SEND;
  TWCR = _BV(TWINT) | _BV(TWSTA) | _BV(TWEN) | _BV(TWIE); // Send START condition.
  return I2C_STATE_FREE;
}

unsigned char i2c_recive( char addr, unsigned char* buf, unsigned char size, void (*callback)(unsigned char)) {
  if (i2c_state) return i2c_state;
  i2c_dev_addr = addr;
  i2c_buf = buf;
  i2c_buf_pos = 0;
  i2c_size = size;
  i2c_callback = callback;
  i2c_state = I2C_STATE_RECIVE;
  TWCR = _BV(TWINT) | _BV(TWSTA) | _BV(TWEN) | _BV(TWIE); // Send START condition.
  return I2C_STATE_FREE;
}

void i2c_send_isp(unsigned char state) {
  switch(state) {
    case TW_START : //Шина I2C переведена в состояние start transmission. Запрашиваем устройство.
      TWDR = i2c_dev_addr;
      TWCR = _BV(TWINT) | _BV(TWEN) | _BV(TWIE); // Load SLA+W into TWDR Register. Clear TWINT bit in TWCR to start transmission of address.
      break;
    case TW_MT_SLA_ACK : // Устройство ответело, ожидает данные.
      TWDR = i2c_buf[i2c_buf_pos++];
      TWCR = _BV(TWINT) | _BV(TWEN) | _BV(TWIE);
      break;
    case TW_MT_DATA_ACK : // Данные успешно переданы.
      if (i2c_buf_pos < i2c_size) { // Если в буфере есть данные - продолжаем передачу
        TWDR = i2c_buf[i2c_buf_pos++];
        TWCR = _BV(TWINT) | _BV(TWEN) | _BV(TWIE);
      } else { // Если в буфере данных нет завершаем передачу
        TWCR = _BV(TWINT) | _BV(TWEN) | _BV(TWSTO); // Завершение передача
        i2c_state = I2C_STATE_FREE;
        if (i2c_callback != 0) i2c_callback(state);
      }
      break;
    case TW_MT_SLA_NACK : 
      if (i2c_buf_pos < i2c_size) { // Если в буфере есть данные - продолжаем передачу
        TWDR = i2c_buf[i2c_buf_pos++];
        TWCR = _BV(TWINT) | _BV(TWEN) | _BV(TWIE);
      } else { // Если в буфере данных нет завершаем передачу
        TWCR = _BV(TWINT) | _BV(TWEN) | _BV(TWSTO); // Завершение передача
        i2c_state = I2C_STATE_FREE;
        if (i2c_callback != 0) i2c_callback(state);
      }
    default :
      TWCR = 0; // Завершение передача
      i2c_state = I2C_STATE_FREE;
      if (i2c_callback != 0) i2c_callback(state);
  }
}

void i2c_recive_isp(unsigned char state) {
  switch(state) {
    case TW_START : //Шина I2C переведена в состояние start transmission. Запрашиваем устройство.
      TWDR = i2c_dev_addr;
      TWCR = _BV(TWINT) | _BV(TWEN) | _BV(TWIE); // Load SLA+W into TWDR Register. Clear TWINT bit in TWCR to start transmission of address.
      break;
    case TW_REP_START : //Шина I2C переведена в состояние start transmission. Запрашиваем устройство.
      TWCR = _BV(TWINT) | _BV(TWEN) | _BV(TWIE);
      break;
    case TW_MR_SLA_ACK : // Устройство ответело, готово слать данные.
        TWCR = _BV(TWINT) | _BV(TWEN) | _BV(TWIE);
      break;
/*    case TW_MR_SLA_NACK : // Устройство ответело, готово слать данные.
        TWCR = _BV(TWINT) | _BV(TWEN) | _BV(TWIE);
      break;*/
    case TW_MR_DATA_ACK : // Устройство ответело, пришел байт данных.
      i2c_buf[i2c_buf_pos++] = TWDR;
      if (i2c_buf_pos < i2c_size) {
        TWCR = _BV(TWINT) | _BV(TWEN) | _BV(TWIE);
      } else {
        TWCR = _BV(TWINT) | _BV(TWSTO) | _BV(TWIE); // Завершение передача
        i2c_state = I2C_STATE_FREE;
        i2c_callback(state);
      }
      break;
    case TW_MR_DATA_NACK : // Устройство ответело, пришел байт данных.
      i2c_buf[i2c_buf_pos++] = TWDR;
      if (i2c_buf_pos < i2c_size) {
        TWDR = i2c_dev_addr;
        TWCR = _BV(TWINT) | _BV(TWSTA) | _BV(TWEN) | _BV(TWIE); // Рестарт для следующего байта
      } else {
        TWCR = _BV(TWINT) | _BV(TWEN) | _BV(TWSTO); // Завершение передача
        i2c_state = I2C_STATE_FREE;
        i2c_callback(state);
      }
      break;
    case TW_MT_SLA_NACK : //Мы такие быстрые, что шина не сбросилась после передачи - сбрасываем. 
      TWDR = i2c_dev_addr;
      TWCR = _BV(TWINT) | _BV(TWSTA) | _BV(TWEN) | _BV(TWIE); // Рестарт для следующего байта
      break;
    default :
      TWCR = 0; // Завершение передача
      i2c_state = I2C_STATE_FREE;
      i2c_callback(state);
  }
}

ISR (TWI_vect) {
  cli();
  switch (i2c_state) {
    case I2C_STATE_SEND : i2c_send_isp(TWSR & 0xF8); break;
    case I2C_STATE_RECIVE : i2c_recive_isp(TWSR & 0xF8); break;
    default : {
      TWCR = 0; // Завершение передача
      i2c_state = I2C_STATE_FREE;
    }
  }
  sei();
}
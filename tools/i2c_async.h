#ifndef __i2c_async_H_
#define __i2c_async_H_
#include <util/twi.h>
#include <avr/io.h>
#include <util/twi.h>
#include <avr/interrupt.h>
#include "uart_async.h"
#include "common.h"
#include "timer16.h"
#define I2C_STATE_FREE 0x00
#define I2C_STATE_STOPPING 0x01
#define I2C_STATE_INOUT 0x03
#define SDA_WAIT_LEVEL PCMSK1 |= _BV(PCINT12)
#define SDA_CHECK_LEVEL PORTC &= _BV(PC4)

unsigned char* i2c_result;
unsigned char i2c_state;

#define HEX_CMD_MAX_SIZE 36 // Длина шеснадцатиричной команды для отправки на I2C
#define HEX_CMD_RECIVE_MAX_SIZE 32 // Длина результата получаемого с I2C (без учета префикса)
struct rec_str_commandI2C {
  byte data[HEX_CMD_MAX_SIZE];
  byte size;
  byte reciveBuf[HEX_CMD_RECIVE_MAX_SIZE];
  byte reciveBufSize;
  byte task;
} commandI2CData;

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
                        byte (*callback)(unsigned char));
void i2c_init(byte divider);

#endif
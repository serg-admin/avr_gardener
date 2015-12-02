#ifndef __i2c_async_H_
#define __i2c_async_H_
#include <util/twi.h>
#define I2C_STATE_FREE 0x00
#define I2C_STATE_SEND 0x01
#define I2C_STATE_RECIVE 0x02
#define I2C_STATE_INOUT 0x03
#define DS3231_ADDRESS 0xD0 // Модуль реального времени.
#define AT24C32_ADDRESS 0xAE // eeprom 4 килобайта 24C32N

unsigned char* i2c_result;

unsigned char i2c_send(char addr, unsigned char* buf, unsigned char size, void (*callback)(unsigned char));
unsigned char i2c_recive( char addr, unsigned char* buf, unsigned char size, void (*callback)(unsigned char));
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
                        void (*callback)(unsigned char));
void i2c_init(void);

#endif
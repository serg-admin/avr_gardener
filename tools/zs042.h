#ifndef __ZS042_H_
#define __ZS042_H_ 1
#include <timer16.h>

// Идентификаторы  I2C
#define DS3231_ADDRESS 0xD0 // Модуль реального времени.
#define AT24C32_ADDRESS 0xAE // eeprom 4 килобайта 24C32N

// ==== Структура памяти AT24C32 ====

// Блок ежеминутных задач
// Представляет собой массив структур <un_alarm_to_array> из timer16.h
#define AT24C32_MINUTELE_BLOCK_START 0x00
#define AT24C32_MINUTELE_BLOCK_LEN sizeof(un_alarm_to_array)*0x20

// Блок "будильников"
// Представляет собой массив усеченных структур <rec_alarm> из timer16.h
#define AT24C32_ALARMS_BLOCK_START (AT24C32_MINUTELE_BLOCK_START + AT24C32_MINUTELE_BLOCK_LEN)
#define AT24C32_ALARMS_BLOCK_BYTES_BY_RECORD 16
#define AT24C32_ALARMS_BLOCK_MAX_REC_COUNT 16
#define AT24C32_ALARMS_BLOCK_LEN (AT24C32_ALARMS_BLOCK_BYTES_BY_RECORD * AT24C32_ALARMS_BLOCK_MAX_REC_COUNT)
#endif
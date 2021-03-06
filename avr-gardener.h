#ifndef __AVR_GARDENER_H_
#define __AVR_GARDENER_H_ 1
#include "tools/common.h"
#include "tools/queue_tasks.h"
// Идентификаторы задач для главного цикла
#define DO_REFRESH_TIME         0x20
#define DO_PRINT_TIME           0x21
#define DO_FETCH_DAILY_ALARM    0x25 // Активирует выполнение будильников текущей минуты
#define DO_COMMAND_INOUT_I2C    0x30 // Выполнить сценарий для I2C
#define DO_TOUCH_RELAY_A        0x31
#define DO_LOAD_ALARM           0x35 // Читает будильник и выводит параметры в USART
#define DO_COMMAND_CLEAN_24C32N 0xE0 // Очистить eeprom 24C32N
#define DO_COMMAND_SET_SPEED    0xE1 // Устанавливает стартовое значение Timer1

#define ALARM_EXEC 0
#define ALARM_SHOW 1

extern void eeprom_24C32N_clean(byte* adr);

void commands_reciver(char* str);

#endif
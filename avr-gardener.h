#include "tools/common.h"
#include "tools/queue_tasks.h"
// Идентификаторы задач для главного цикла
#define DO_REQUEST_RTC_DATA_START 0x20
#define DO_REQUEST_RTC_DATA_END 0x21
#define DO_COMMAND_INOUT_I2C 0x30 // Выполнить сценарий для I2C
#define DO_COMMAND_CLEAN_24C32N 0xE0 // Очистить eeprom 24C32N
#define DO_TOUCH_RELAY_A 0x31
#define DO_LOAD_ALARM 0x35

#define HEX_CMD_MAX_SIZE 36 // Длина шеснадцатиричной команды для отправки на I2C
#define HEX_CMD_RECIVE_MAX_SIZE 32 // Длина результата получаемого с I2C (без учета префикса)
struct rec_str_commandI2C {
  byte data[HEX_CMD_MAX_SIZE];
  byte size;
  byte reciveBuf[HEX_CMD_RECIVE_MAX_SIZE];
  byte reciveBufSize;
} commandI2CData;

extern void eeprom_24C32N_clean(byte* adr);
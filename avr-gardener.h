#include "tools/common.h"
#include "tools/queue_tasks.h"
// Идентификаторы задач для главного цикла
#define DO_COMMAND_INOUT_I2C 0x30 // Выполнить сценарий для I2C
#define DO_COMMAND_CLEAN_24C32N 0xE0 // Очистить eeprom 24C32N

#define HEX_CMD_MAX_SIZE 36 // Длина шеснадцатиричной команды для отправки на I2C
#define HEX_CMD_RECIVE_MAX_SIZE 32 // Длина результата получаемого с I2C (без учета префикса)
struct rec_str_commandI2C {
  byte data[HEX_CMD_MAX_SIZE];
  byte size;
  byte reciveBuf[HEX_CMD_RECIVE_MAX_SIZE];
  byte reciveBufSize;
} commandI2CData;

struct rec_data_time { // 7 байт
  byte second;
  byte minut;
  byte hour;
  byte dayOfMonth;
  byte month;
  byte year;
  byte dayOfWeek;
} current_time;

struct rec_byte_time_flags {
  // Флажки - какие поля проверять для срабатывания
  byte second:1;
  byte minut:1;
  byte hour:1;
  byte dayOfMonth:1;
  byte month:1;
  byte year:1;
  byte dayOfWeek:1;
  byte enable:1;  
};

union un_byte_time_flags {
  byte b;
  struct rec_byte_time_flags f;
};

struct rec_alarm { // 24 байта
  struct rec_data_time alarm_time; // 7 байт
  struct rec_data_time last_alarm_time; // 7 байт
  union un_byte_time_flags flags; // 1 байт. Флажки - какие поля проверять для срабатывания
  byte reserv1;
  struct rec_alarm_task task;
  //==  Не сохраняемые данные ===
  struct rec_data_time next_alarm_time; // Храним только в памяти
  byte num; // номер будильника
};

union un_alarm_to_array {
  struct rec_alarm alarm;
  byte arr[32];
};

extern void eeprom_24C32N_clean(byte* adr);
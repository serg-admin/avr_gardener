#ifndef __TIMER16_H_
#define __TIMER16_H_ 1
#include <avr/io.h>
#include "queue_tasks.h"


struct rec_data_time { // 6 байт
  byte minut;
  byte hour;
  byte dayOfMonth;
  byte month;
  byte dayOfWeek;
} current_time;

struct rec_byte_time_flags {
  // Флажки - какие поля проверять для срабатывания
  byte reserved1:1; // (second) Нет посекндным будильникам
  byte minut:1; // (minut) Ежеминутный будильник будет специальной отдельной записью (минуты проверяем всегда)
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
  struct rec_data_time alarm_time; // 5 байт
  struct rec_data_time last_alarm_time; // 5 байт
  union un_byte_time_flags flags; // 1 байт. Флажки - какие поля проверять для срабатывания
  struct rec_task task; // 3 байт
  //==  Не сохраняемые данные ===
  struct rec_data_time next_alarm_time; // Храним только в памяти
  byte num; // номер будильника
};

union un_alarm_to_array {
  struct rec_alarm alarm;
  byte arr[32];
};

void timer_init();
void timer1PutTask(uint16_t delay, void (*func)(uint8_t*), uint8_t* data);


#endif
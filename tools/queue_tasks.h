#ifndef __ERRNO_H_
#define __ERRNO_H_ 1

#include "common.h"

#define QUEUE_TASKS_SIZE 10
#define DO_TIMER1_OVF 0x01
#define DO_REQUEST_RTC_DATA_START 0x20
#define DO_REQUEST_RTC_DATA_END 0x21
struct rec_alarm_task {
  byte task_id;
  byte data[2];
} queue_tasks_current;

void queue_init(void);
unsigned char queue_getTask();
void queue_putTask(unsigned char task);
void queue_putTask2b(byte task, byte data1, byte data2);
#endif
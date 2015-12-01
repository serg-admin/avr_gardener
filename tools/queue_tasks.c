#include "queue_tasks.h"
#include <avr/interrupt.h>
// Очередь задачь
unsigned char queue_tasks[QUEUE_TASKS_SIZE];
unsigned char queue_tasks_rpos;
unsigned char queue_tasks_wpos;

void queue_init(void) {
  queue_tasks_rpos = 0;
  queue_tasks_wpos = 0;
  queue_tasks_current = 0;
}

unsigned char queue_getTask() {
  if (queue_tasks_rpos == queue_tasks_wpos) return 0;
  queue_tasks_current = queue_tasks[queue_tasks_rpos++];
  if (queue_tasks_rpos >= QUEUE_TASKS_SIZE) queue_tasks_rpos = 0;
  return queue_tasks_current;
}

void queue_putTask(unsigned char task) {
  cli();
  queue_tasks[queue_tasks_wpos++] = task;
  if (queue_tasks_wpos >= QUEUE_TASKS_SIZE) queue_tasks_wpos = 0;
  sei();
}
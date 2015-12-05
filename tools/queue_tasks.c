#include "queue_tasks.h"
#include <avr/interrupt.h>
// Очередь задачь
struct rec_task queue_tasks[QUEUE_TASKS_SIZE];
unsigned char queue_tasks_rpos;
unsigned char queue_tasks_wpos;

void queue_init(void) {
  queue_tasks_rpos = 0;
  queue_tasks_wpos = 0;
  queue_tasks_current.task_id = 0;
}

unsigned char queue_getTask() {
  if (queue_tasks_rpos == queue_tasks_wpos) return 0;
  queue_tasks_current = queue_tasks[queue_tasks_rpos++];
  if (queue_tasks_rpos >= QUEUE_TASKS_SIZE) queue_tasks_rpos = 0;
  return queue_tasks_current.task_id;
}

void queue_putTask2b(byte task, byte data1, byte data2) {
  cli();
  queue_tasks[queue_tasks_wpos].task_id = task;
  queue_tasks[queue_tasks_wpos].data[0] = data1;
  queue_tasks[queue_tasks_wpos++].data[1] = data2;
  if (queue_tasks_wpos >= QUEUE_TASKS_SIZE) queue_tasks_wpos = 0;
  sei();  
}

void queue_putTask(unsigned char task) {
  queue_putTask2b(task , 0, 0);

}

#define QUEUE_TASKS_SIZE 10
#define DO_TIMER1_OVF 0x01
#define DO_REQUEST_RTC_DATA_START 0x20
#define DO_REQUEST_RTC_DATA_END 0x21
unsigned char queue_tasks_current;

void queue_init(void);
unsigned char queue_getTask();
void queue_putTask(unsigned char task);
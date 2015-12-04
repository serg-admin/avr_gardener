#ifndef __TIMER16_H_
#define __TIMER16_H_ 1
#include <avr/io.h>

void timer_init();
void timer1PutTask(uint16_t delay, void (*func)(uint8_t*), uint8_t* data);
#endif
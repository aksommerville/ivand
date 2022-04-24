/* timed_tasks.h
 * Coordinates random-ish activity tied to the timer.
 * For now that is only deliveries to the truck.
 */
 
#ifndef TIMED_TASKS_H
#define TIMED_TASKS_H

#include <stdint.h>

extern uint8_t taskc;

void timed_tasks_init();
void timed_tasks_update();

#endif

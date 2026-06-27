#ifndef TASKS_H
#define TASKS_H

#include "freertos/FreeRTOS.h"

void task_evaluate_timers(void *args);
void task_check_io(void *args);
void task_check_cfg(void *args);
void task_operate(void *args);

#endif //TASKS_H

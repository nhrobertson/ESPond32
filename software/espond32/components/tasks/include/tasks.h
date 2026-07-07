#ifndef TASKS_H
#define TASKS_H

#include "freertos/FreeRTOS.h"
#include "device.h"
#include "models.h"
#include "config.h"
#include "freertos/idf_additions.h"
#include "freertos/projdefs.h"
#include "filesystem.h"
#include "float.h"

//Task Master for RTOS
//
// Basic functionality of device:
//
// Check State of all GPIO devices (on/off switches | float sens)
// Check to see if the CFG has changed
// See if the time has passed the configured timers for automatic control
// Operate based on configuration

void task_check_for_reset(void *args);
void task_check_leak(void *args);
void task_check_io(void *args);
void task_check_cfg(void *args);
void task_evaluate_cfg(void *args);
void task_operate(void *args);

#endif //TASKS_H

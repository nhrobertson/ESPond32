#ifndef FLOAT_H
#define FLOAT_H

#include <stdbool.h>
#include "models.h"
#include "date_and_time.h"
#include "esp_timer.h"

bool eval_float_state(device_t *dev);
bool check_leak_check();
bool check_fills_per_day();
void clear_leak();

#endif //FLOAT_H

#ifndef DATE_AND_TIME_H
#define DATE_AND_TIME_H

#include <time.h>
#include "models.h"
#include <string.h>

struct tm get_local_time();

dev_op_state_t evaluate_device_schedule(device_t *dev);

#endif //DATE_AND_TIME_H


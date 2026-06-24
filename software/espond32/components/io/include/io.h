#ifndef IO_H
#define IO_H

#include <stdio.h>
#include <inttypes.h>

#include "esp_check.h"
#include "hal/gpio_types.h"
#include "soc/gpio_num.h"
#include "driver/gpio.h"


//General Purpose Accessible Functions
esp_err_t   configure_gpio(gpio_num_t GPIO_NUM, gpio_mode_t GPIO_MODE);
esp_err_t   disable_gpio(gpio_num_t GPIO_NUM);



#endif //IO_H

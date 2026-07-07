#ifndef IO_H
#define IO_H

#include <stdio.h>
#include <inttypes.h>

#include "esp_check.h"
#include "hal/gpio_types.h"
#include "soc/gpio_num.h"
#include "driver/gpio.h"
#include "models.h"


//General Purpose Accessible Functions
esp_err_t configure_gpio(gpio_num_t GPIO_NUM, gpio_mode_t GPIO_MODE, gpio_int_type_t INTR_TYPE);
esp_err_t set_gpio(gpio_num_t GPIO_NUM, gpio_level_t level);
esp_err_t disable_gpio(gpio_num_t GPIO_NUM);

esp_err_t io_setup(device_t *device);
esp_err_t io_operate(device_t *device, io_mode_t mode);
esp_err_t io_disable(device_t *device);

esp_err_t setup_reset_button();

extern const device_ops_t o_ops;
extern const device_ops_t i_ops;

#endif //IO_H

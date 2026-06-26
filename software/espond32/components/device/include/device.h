#ifndef DEVICE_H
#define DEVICE_H

#include <stdint.h>
#include <stdbool.h>

#include "config.h"
#include "models.h"
#include "io.h"
#include "led.h"

#include "driver/gpio.h"
#include "esp_err.h"
#include "soc/gpio_num.h"

/**
 *  device.h
 *  
 *  Abstraction Layer for utilizing the various devices in the system
 *
 *  A Device in this system is defined as a peripheral which is actively either:
 *    a. monitored
 *    b. driven
 *  which provides main functioning to the system
 *
 *  There are three main devices for this system:
 *  1. Pump 1
 *  2. Pump 2
 *  3. Valve 1
 *
 *  There is also a input monitored device:
 *  1. Float Sensor 1
 **/


//Set according to your specifications
static const output_pins_t OUTPUT_PINS[NUM_OUTPUTS] = {
  { .ssr = PUMP1_SSR_GPIO,  .sw_a = PUMP1_SW_A_GPIO,  .sw_b = PUMP1_SW_B_GPIO,  .led_pixel = PUMP1_LED_PIX },
  { .ssr = PUMP2_SSR_GPIO,  .sw_a = PUMP2_SW_A_GPIO, .sw_b = PUMP2_SW_B_GPIO, .led_pixel = PUMP2_LED_PIX },
  { .ssr = VALVE1_SSR_GPIO, .sw_a = VALVE1_SW_A_GPIO, .sw_b = VALVE1_SW_B_GPIO,  .led_pixel = VALVE1_LED_PIX }
};

static const input_pins_t INPUT_PINS = {
  .float_sens = FLOAT1_SENS_GPIO,
  .led_pixel  = FLOAT1_LED_PIX
};

//Global Device List
extern device_t g_devices[NUM_DEVICES];

//Functions
DEVICE_RET get_device(char* name);

INIT_RET devices_init(void);

#endif //DEVICE_H

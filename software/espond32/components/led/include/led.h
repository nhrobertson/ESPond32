#ifndef LED_H
#define LED_H

#include "esp_err.h"
#include <inttypes.h>
#include <stdbool.h>
#include "models.h"
#include "led_strip.h"

//Handles Blinking aswell
static pix_t get_color(led_state_t state, uint64_t now); 
LED_INIT_RET attach_led(device_t *device);


#endif //LED_H

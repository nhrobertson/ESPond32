#ifndef LED_H
#define LED_H

#include "esp_err.h"
#include <inttypes.h>
#include <stdbool.h>
#include "models.h"
#include "led_strip.h"
#include "config.h"

const pix_t RED       = {255, 0, 0};
const pix_t GREEN     = {0, 255, 0};
const pix_t BLUE      = {0, 255, 0};
const pix_t BLACK     = {0, 0, 0};
const pix_t YELLOW    = {255, 255, 0};
const pix_t AMBER     = {255, 128, 0};


static pix_t get_color(led_state_t state, uint64_t now); 

LED_INIT_RET leds_init();

extern const led_ops_t led_ops; 


#endif //LED_H

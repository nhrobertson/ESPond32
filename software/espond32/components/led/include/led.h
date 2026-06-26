#ifndef LED_H
#define LED_H

#include "esp_err.h"
#include <inttypes.h>
#include <stdbool.h>
#include "models.h"
#include "led_strip.h"
#include "config.h"

extern const pix_t PIX_RED;
extern const pix_t PIX_GREEN;
extern const pix_t PIX_BLUE;
extern const pix_t PIX_BLACK;
extern const pix_t PIX_YELLOW;
extern const pix_t PIX_AMBER;


static pix_t get_color(led_state_t state, uint64_t now); 

LED_INIT_RET leds_init();

extern const led_ops_t dev_led_ops; 


#endif //LED_H

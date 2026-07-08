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


LED_INIT_RET leds_init();
esp_err_t set_sys_led_color(pix_t pixel);

extern const led_ops_t dev_led_ops; 


#endif //LED_H

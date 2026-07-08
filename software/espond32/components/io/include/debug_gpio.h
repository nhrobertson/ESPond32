#ifndef DEBUG_GPIO_H
#define DEBUG_GPIO_H

#include "config.h"
#include "driver/gpio.h"

#if DEBUG_GPIO_ENABLE

static inline void debug_gpio_init(void) {
  const gpio_num_t pins[] = {
    DEBUG_GPIO_OPERATE, DEBUG_GPIO_EVAL, DEBUG_GPIO_IO,
    DEBUG_GPIO_LEAK, DEBUG_GPIO_RESET, DEBUG_GPIO_NET
  };
  for (size_t i = 0; i < sizeof(pins) / sizeof(pins[0]); ++i) {
    gpio_reset_pin(pins[i]);
    gpio_set_direction(pins[i], GPIO_MODE_OUTPUT);
    gpio_set_level(pins[i], 0);
  }
}

static inline void debug_pin_set(gpio_num_t pin, int level) {
  gpio_set_level(pin, level);
}

#else

static inline void debug_gpio_init(void) {}
static inline void debug_pin_set(gpio_num_t pin, int level) { (void)pin; (void)level; }

#endif

#endif //DEBUG_GPIO_H

#ifndef MODELS_H
#define MODELS_H

#include <inttypes.h>
#include "esp_err.h"
#include "soc/gpio_num.h"

//Device Type Definetions
typedef enum device_type_t {
  DEV_PUMP,
  DEV_VALVE,
  DEV_FLOAT
} device_type_t;

typedef struct output_pins_t {
  gpio_num_t ssr;
  gpio_num_t sw_a, sw_b;
  uint8_t led_pixel;
} output_pins_t;

typedef struct input_pins_t {
  gpio_num_t float_sens;
  uint8_t led_pixel;
} input_pins_t;

typedef enum switch_pos_t {
  SW_ON,
  SW_AUTO,
  SW_OFF
} switch_pos_t;

typedef enum gpio_level_t {
  GPIO_OFF = 0,
  GPIO_ON = 1,
  GPIO_SET,
} gpio_mode_t;

typedef struct io_mode_t {
  enum gpio_level_t state;
  float value;
} io_mode_t;

typedef struct device device_t;

typedef struct device_ops_t {
  esp_err_t (*setup)   (device_t *dev);
  esp_err_t (*operate) (device_t *dev, io_mode_t mode);
  esp_err_t (*disable) (device_t *dev);
} device_ops_t;

//LED Type Definitions
typedef struct pix_t {
  uint8_t r, g, b;
} pix_t;

typedef enum led_state_t {
  LED_ON,
  LED_ACTIVE,
  LED_OFF,
  LED_FAULT
} led_state_t;

typedef struct led_ops_t {
  esp_err_t (*set_color)  (device_t *dev, pix_t pixel);
} led_ops_t;

typedef struct led_t {
  led_state_t state;
  const led_ops_t *ops;
} led_t;


//Return Type Definitions
typedef struct DEVICE_RET {
  device_t *device;
  esp_err_t ret_status;
} DEVICE_RET;

typedef struct INIT_RET {
  char *dev;
  esp_err_t ret_status;
} INIT_RET;

typedef struct LED_INIT_RET {
  char *name;
  esp_err_t ret_status;
} LED_INIT_RET;

#endif //MODELS_H

#ifndef MODELS_H
#define MODELS_H

#include <stdbool.h>
#include <inttypes.h>
#include "esp_err.h"
#include "freertos/idf_additions.h"
#include "soc/gpio_num.h" 
#include "config.h"
#include "cJSON.h"



//Device Type Definetions
typedef enum device_type_t {
  DEV_PUMP,
  DEV_VALVE,
  DEV_LIGHT,
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

typedef enum auto_mode_override_t {
  OVR_ON,
  OVR_AUTO,
  OVR_OFF,
} auto_mode_override_t;

typedef enum switch_pos_t {
  SW_ON,
  SW_AUTO,
  SW_OFF
} switch_pos_t;

typedef enum gpio_level_t {
  GPIO_OFF = 0,
  GPIO_ON = 1,
  GPIO_GET,
} gpio_level_t;

typedef struct io_mode_t {
  enum gpio_level_t state;
  float value;
} io_mode_t;

typedef struct debounce_t {
  int canditate;
  uint8_t count;
  int stable;
  bool init;
} debounce_t;

typedef struct device device_t;

typedef struct device_ops_t {
  esp_err_t (*setup)   (device_t *dev);
  esp_err_t (*operate) (device_t *dev, io_mode_t *mode);
  esp_err_t (*check)   (device_t *dev);
  esp_err_t (*disable) (device_t *dev);
} device_ops_t;

typedef enum dev_op_state_t {
  DEV_STATE_ON,
  DEV_STATE_OFF,
  DEV_STATE_AUTO,
  DEV_STATE_ERR
} dev_op_state_t;

typedef enum fill_state_t {
  FILL_IDLE,
  FILL_ARMING,
  FILL_FILLING,
  FILL_OVERFLOW,
} fill_state_t;

typedef struct leak_check_t {
  uint8_t d;
  uint8_t count;
} leak_check_t;

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

extern struct device {
  const char *name;
  device_type_t type;
  const device_ops_t *ops;
  led_t led;
  const void *pins;
  union {
    struct out {
      debounce_t sw_a_db;
      debounce_t sw_b_db;
      switch_pos_t sw;
      auto_mode_override_t auto_override;
      dev_op_state_t state;
      bool         on;
    } out;
    struct in {
      debounce_t float_sens_db;
      fill_state_t state;
      bool         active;
    } in;
  } u;
} device;

//Network Helpers
typedef struct status_json_builder_t {
  int state;
  char name[16];
} status_json_builder_t;

typedef struct override_json_t {
  char name[16];
  auto_mode_override_t override;
} override_json_t;


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

//Configuration Models
typedef struct output_schedule_t {
  char name[16];
  uint8_t on_hour;
  uint8_t on_min;
  uint8_t off_hour;
  uint8_t off_min;
  uint8_t days_mask;
  bool has_schedule;
} output_schedule_t;

typedef struct float_cfg_t {
  uint8_t threshold_min;
  uint8_t overflow_min;
  uint8_t max_fills_per_day;
} float_cfg_t;

typedef struct espond_cfg_t {
  uint16_t version;
  uint8_t num_outputs;
  output_schedule_t outputs[NUM_OUTPUTS];
  float_cfg_t float_sens;
} espond_cfg_t;

//RTOS variables
#define CFG_CHANGED_BIT BIT0
#define REQ_CLEAR_LEAK BIT1
extern EventGroupHandle_t g_events;
extern SemaphoreHandle_t cfg_buff_mutex;
extern SemaphoreHandle_t cfg_change_mutex;
extern SemaphoreHandle_t ovr_change_mutex;
extern SemaphoreHandle_t lockout_change_mutex;
extern SemaphoreHandle_t leak_check_mutex;
extern TaskHandle_t operate_handle;
extern TaskHandle_t task_handle;


extern volatile espond_cfg_t g_espond_cfg;
extern espond_cfg_t g_buff_cfg;
extern bool lockout;

esp_err_t parse_config_json(const char *config_str, espond_cfg_t *out);
esp_err_t parse_override_json(const char *json_str, override_json_t *out);

#endif //MODELS_H

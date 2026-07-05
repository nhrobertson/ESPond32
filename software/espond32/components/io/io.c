#include "io.h"
#include "driver/gpio.h"
#include "esp_err.h"
#include "hal/gpio_types.h"
#include "models.h"

static const char *TAG = "espond32 - IO";

esp_err_t configure_gpio(gpio_num_t GPIO_NUM, gpio_mode_t GPIO_MODE, gpio_int_type_t INTR_TYPE)
{
  esp_err_t ret;
  ESP_LOGI(TAG, "Configure: GPIO_NUM %d", GPIO_NUM);
  //Reset the Pin before configuring I/O direction
  ret = gpio_reset_pin(GPIO_NUM);
  if (ret != ESP_OK) { return ret; }

  //Set I/O direction
  ret = gpio_set_direction(GPIO_NUM, GPIO_MODE);
  if (ret != ESP_OK) { return ret; }
  
  //Enable Interrupt if input and enabled
  if (GPIO_MODE == GPIO_MODE_INPUT && INTR_TYPE != GPIO_INTR_DISABLE) {
    ret = gpio_set_intr_type(GPIO_NUM, INTR_TYPE);
    if (ret != ESP_OK) { return ret; }
    ret = gpio_intr_enable(GPIO_NUM);
  }

  return ret;
}

esp_err_t disable_gpio(gpio_num_t GPIO_NUM) {
  esp_err_t ret;
  ESP_LOGI(TAG, "Disable: GPIO_NUM %d", GPIO_NUM);

  ret = gpio_reset_pin(GPIO_NUM);
  if (ret != ESP_OK) { return ret; }

  ret = gpio_set_direction(GPIO_NUM, GPIO_MODE_DISABLE);
  return ret;
}

esp_err_t set_gpio(gpio_num_t GPIO_NUM, gpio_level_t level) {
  esp_err_t ret;
  ESP_LOGI(TAG, "Set GPIO: %d to %d", GPIO_NUM, (int)level);
  ret = gpio_set_level(GPIO_NUM, (uint32_t)level);
  return ret;
}

esp_err_t read_gpio(gpio_num_t GPIO_NUM, io_mode_t *mode) {
  esp_err_t ret;
  int level = -1;
  level = gpio_get_level(GPIO_NUM);
  ESP_LOGI(TAG, "Read GPIO: %d to %d", GPIO_NUM, (int)level);

  if (level == -1) {
    ret = ESP_ERR_INVALID_RESPONSE;
  } else {
    ret = ESP_OK;
  }

  mode->value = level;
  mode->state = level;
  return ret;
}

esp_err_t io_setup(device_t *device) {
  esp_err_t ret = ESP_ERR_INVALID_ARG;
  switch (device->type) {
    case (DEV_PUMP):
    case (DEV_VALVE):
    case (DEV_LIGHT):
      gpio_num_t ssr = ((const output_pins_t*)device->pins)->ssr;
      gpio_num_t sw_a = ((const output_pins_t*)device->pins)->sw_a;
      gpio_num_t sw_b = ((const output_pins_t*)device->pins)->sw_b;
      
      ret = configure_gpio(ssr, GPIO_MODE_OUTPUT, GPIO_INTR_DISABLE);
      if (ret != ESP_OK) { return ret; }

      ret = configure_gpio(sw_a, GPIO_MODE_INPUT, GPIO_INTR_HIGH_LEVEL);
      if (ret != ESP_OK) { return ret; }
      
      ret = configure_gpio(sw_b, GPIO_MODE_INPUT, GPIO_INTR_HIGH_LEVEL);
      if (ret != ESP_OK) { return ret; }
      
      break;
    case (DEV_FLOAT):
      gpio_num_t float_sens = ((const input_pins_t*)device->pins)->float_sens;
      
      ret = configure_gpio(float_sens, GPIO_MODE_INPUT, GPIO_INTR_HIGH_LEVEL);

      break;
  }
  
  return ret;
}

esp_err_t input_operate(device_t *device, io_mode_t *mode) {
  esp_err_t ret = ESP_ERR_INVALID_ARG;

  gpio_num_t float_sens = ((const input_pins_t*)device->pins)->float_sens;
  
  switch (mode->state) {
    case(GPIO_ON): 
    case(GPIO_OFF):
      ret = ESP_ERR_INVALID_ARG;
      break;
    case(GPIO_GET):
      ret = read_gpio(float_sens, mode);
      break;
  }
  return ret;
}

esp_err_t output_operate(device_t *device, io_mode_t *mode) {
  esp_err_t ret = ESP_ERR_INVALID_ARG;

  gpio_num_t ssr = ((const output_pins_t*)device->pins)->ssr;
  bool on = device->u.out.on;
  
  switch (mode->state) {
    case(GPIO_ON): 
      ret = set_gpio(ssr, mode->state);
      break;
    case(GPIO_OFF):
      ret = set_gpio(ssr, mode->state);
      break;
    case(GPIO_GET):
      ret = ESP_ERR_INVALID_ARG;
      break;
  }
  return ret;
}

esp_err_t output_check(device_t *device) {
  esp_err_t ret;
  //Check the Output Device's Switches
  gpio_num_t sw_a = ((const output_pins_t*)device->pins)->sw_a;
  gpio_num_t sw_b = ((const output_pins_t*)device->pins)->sw_b;
  
  io_mode_t sw_a_state;
  io_mode_t sw_b_state;

  ret = read_gpio(sw_a, &sw_a_state);
  if (ret != ESP_OK) { return ret; }

  ret = read_gpio(sw_b, &sw_b_state);
  if (ret != ESP_OK) { return ret; }

  //Impossible for the Switch A and Switch B to be on at the same time
  if (sw_a_state.state) {
    //On
    device->u.out.sw = SW_ON;
  } else if (sw_b_state.state) {
    //Off
    device->u.out.sw = SW_OFF;
  } else {
    //Auto
    device->u.out.sw = SW_AUTO;
  }

  return ret;
}

esp_err_t input_check(device_t *device) {
  esp_err_t ret;
  //Check the input device's level
  
  gpio_num_t float_sens = ((const input_pins_t*)device->pins)->float_sens;

  io_mode_t float_sens_state;

  ret = read_gpio(float_sens, &float_sens_state);
  if (ret != ESP_OK) { return ret; }

  if (float_sens_state.state) {
    device->u.in.active = true;
  } else {
    device->u.in.active = false;
  }

  return ret;
}

esp_err_t io_disable(device_t *device) {
  esp_err_t ret = ESP_ERR_INVALID_ARG;
  switch (device->type) {
    case (DEV_PUMP):
    case (DEV_VALVE):
    case (DEV_LIGHT):
      gpio_num_t ssr = ((const output_pins_t*)device->pins)->ssr;
      gpio_num_t sw_a = ((const output_pins_t*)device->pins)->sw_a;
      gpio_num_t sw_b = ((const output_pins_t*)device->pins)->sw_b;
      
      ret = disable_gpio(ssr);
      if (ret != ESP_OK) { return ret; }

      ret = disable_gpio(sw_a);
      if (ret != ESP_OK) { return ret; }
     
      ret = disable_gpio(sw_b);
      if (ret != ESP_OK) { return ret; }
      
      break;
    case (DEV_FLOAT):
      gpio_num_t float_sens = ((const input_pins_t*)device->pins)->float_sens;
      
      ret = disable_gpio(float_sens);

      break;
  }
  return ret;
}

static esp_err_t channel_setup(device_t *self)  {
  esp_err_t ret;
  ret = io_setup(self);
  if (ret == ESP_OK) {
    //Verify to resolve IO state
  }
  return ret;
}

static esp_err_t input_channel_operate(struct device *self, io_mode_t *mode) {
  esp_err_t ret;
  ret = input_operate(self, mode);
  return ret;
}

static esp_err_t output_channel_operate(device_t *self, io_mode_t *mode) { 
  esp_err_t ret;
  ret = output_operate(self, mode);
  if (ret == ESP_OK) {
    //Resolve State
  } 
  return ret; 
}

static esp_err_t channel_disable(device_t *self) {
  esp_err_t ret;
  ret = io_disable(self);
  if (ret == ESP_OK) {
    //Resolve State
  } 
  return ret;
}

static esp_err_t input_channel_check(device_t *self) {
  esp_err_t ret;
  ret = input_check(self);
  if (ret == ESP_OK) {
    //Resolve State
  } 
  return ret;
}

static esp_err_t output_channel_check(device_t *self) {
  esp_err_t ret;
  ret = output_check(self);
  if (ret == ESP_OK) {
    //Resolve State
  } 
  return ret;
}

const device_ops_t o_ops = {
    .setup  = channel_setup,
    .operate = output_channel_operate,
    .check   = output_channel_check,
    .disable = channel_disable
};

const device_ops_t i_ops = {
    .setup = channel_setup,
    .operate = input_channel_operate,
    .check   = input_channel_check,
    .disable = channel_disable
};

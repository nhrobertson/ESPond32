#include "io.h"
#include "driver/gpio.h"
#include "hal/gpio_types.h"

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
    if (ret != ESP_OK) { return ret; }
  }

  return ret;
}

esp_err_t disable_gpio(gpio_num_t GPIO_NUM) {
  esp_err_t ret;
  ESP_LOGI(TAG, "Disable: GPIO_NUM %d", GPIO_NUM);

  ret = gpio_reset_pin(GPIO_NUM);

  ret = gpio_set_direction(GPIO_NUM, GPIO_MODE_DISABLE);

  return ret;
}

esp_err_t set_gpio(gpio_num_t GPIO_NUM, gpio_level_t level) {
  esp_err_t ret;
  ret = gpio_set_level(GPIO_NUM, (uint32_t)level);
  return ret;
}

esp_err_t io_setup(device_t *device) {
  esp_err_t ret;

  return ret;
}

esp_err_t io_operate(device_t *device, io_mode_t mode) {
  esp_err_t ret;

  gpio_num_t ssr = ((const output_pins_t*)device->pins)->ssr;
  bool on = device->u.out.on;
  
  switch (mode.state) {
    case(GPIO_ON): 
      set_gpio(ssr, mode.state);
      break;
    case(GPIO_OFF):
      set_gpio(ssr, mode.state);
      break;
    case(GPIO_SET):

      break;
  }

  return ret;
}

esp_err_t io_disable(device_t *device) {
  esp_err_t ret;



  return ret;
}

static esp_err_t channel_setup(device_t *self)  {
  /* calls io_set, etc. */ 

}

static esp_err_t channel_operate(device_t *self, io_mode_t mode) { 
  return io_operate(self); 
}

static esp_err_t channel_disable(device_t *self) {

}

const device_ops_t o_ops = {              // defined in the SAME TU as the statics
    .setup  = channel_setup,
    .operate = channel_operate,
    .disable = channel_disable
};

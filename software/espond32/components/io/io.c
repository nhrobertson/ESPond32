#include "io.h"
#include "hal/gpio_types.h"

static const char *TAG = "espond32 - IO";

esp_err_t configure_gpio(gpio_num_t GPIO_NUM, gpio_mode_t GPIO_MODE)
{
  esp_err_t ret;
  ESP_LOGI(TAG, "Configure: GPIO_NUM %d", GPIO_NUM);
  //Reset the Pin before configuring I/O direction
  ret = gpio_reset_pin(GPIO_NUM);

  //Set I/O direction
  ret = gpio_set_direction(GPIO_NUM, GPIO_MODE);

  return ret;
}

esp_err_t disable_gpio(gpio_num_t GPIO_NUM) {
  esp_err_t ret;
  ESP_LOGI(TAG, "Disable: GPIO_NUM %d", GPIO_NUM);

  ret = gpio_reset_pin(GPIO_NUM);

  ret = gpio_set_direction(GPIO_NUM, GPIO_MODE_DISABLE);

  return ret;
}

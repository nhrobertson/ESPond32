#include "led.h"
#include "config.h"
#include "esp_err.h"
#include "led_strip.h"
#include "models.h"


const pix_t PIX_RED       = {.r = 255, .g = 0, .b = 0};
const pix_t PIX_GREEN     = {.r = 0, .g = 255, .b = 0};
const pix_t PIX_BLUE      = {.r = 0, .g = 0, .b = 255};
const pix_t PIX_BLACK     = {.r = 0, .g = 0, .b = 0};
const pix_t PIX_YELLOW    = {.r = 255, .g = 255, .b = 0};
const pix_t PIX_AMBER     = {.r = 255, .g = 128, .b = 0};

led_strip_handle_t led_strip = NULL;

LED_INIT_RET leds_init() {
  LED_INIT_RET ret = {.name = "led", .ret_status = ESP_OK };

  /// LED strip common configuration
  led_strip_config_t strip_config = {
      .strip_gpio_num = LED_DATA_GPIO,  // The GPIO that connected to the LED strip's data line
      .max_leds = NUM_LEDS,                 // The number of LEDs in the strip,
      .led_model = LED_MODEL_WS2812, // LED strip model, it determines the bit timing
      .color_component_format = LED_STRIP_COLOR_COMPONENT_FMT_RGB, // The color component format is G-R-B
      .flags = {
          .invert_out = false, // don't invert the output signal
      }
  };

  /// RMT backend specific configuration
  led_strip_rmt_config_t rmt_config = {
      .clk_src = RMT_CLK_SRC_DEFAULT,    // different clock source can lead to different power consumption
      .resolution_hz = 10 * 1000 * 1000, // RMT counter clock frequency: 10MHz
      .mem_block_symbols = 64,           // the memory size of each RMT channel, in words (4 bytes)
      .flags = {
          .with_dma = false, // DMA feature is available on chips like ESP32-S3/P4
      }
  };

  /// Create the LED strip object
  ESP_ERROR_CHECK(led_strip_new_rmt_device(&strip_config, &rmt_config, &led_strip));
  
  led_strip_clear(led_strip);
  
  return ret;
}

esp_err_t set_sys_led_color(pix_t pixel) {
  esp_err_t ret = ESP_ERR_INVALID_RESPONSE;

  ret = led_strip_set_pixel(led_strip, SYS_LED_PIX, pixel.r, pixel.g, pixel.b);
  ret = led_strip_refresh(led_strip);
  return ret;
}

esp_err_t set_dev_led_color(device_t *device, pix_t pixel) {
  esp_err_t ret = ESP_ERR_INVALID_RESPONSE;
  switch (device->type) {
    case (DEV_PUMP):
    case (DEV_VALVE):
    case (DEV_LIGHT):
      ret = led_strip_set_pixel(led_strip, ((const output_pins_t*)device->pins)->led_pixel, pixel.r, pixel.g, pixel.b);
      break;
    case (DEV_FLOAT):
      ret = led_strip_set_pixel(led_strip, ((const input_pins_t*)device->pins)->led_pixel, pixel.r, pixel.g, pixel.b);
      break;
  }
  ret = led_strip_refresh(led_strip);
  return ret;
}

const led_ops_t dev_led_ops = { .set_color = set_dev_led_color };

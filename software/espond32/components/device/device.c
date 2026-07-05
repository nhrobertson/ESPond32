#include "device.h"
#include "led.h"
#include "models.h"


device_t g_devices[NUM_DEVICES] = {
  { .name = "pump1",  .type = DEV_PUMP,  .ops = &o_ops, .pins = &OUTPUT_PINS[0] },
  { .name = "pump2",  .type = DEV_PUMP,  .ops = &o_ops, .pins = &OUTPUT_PINS[1] },
  { .name = "valve1", .type = DEV_VALVE, .ops = &o_ops, .pins = &OUTPUT_PINS[2] },
  { .name = "light1", .type = DEV_LIGHT, .ops = &o_ops, .pins = &OUTPUT_PINS[3] },
  { .name = "float1", .type = DEV_FLOAT, .ops = &i_ops, .pins = &INPUT_PINS }
};

DEVICE_RET get_device(char* name) {
  esp_err_t ret_status = ESP_ERR_INVALID_ARG;
  device_t *device = NULL;
  DEVICE_RET ret = { .device = device, .ret_status = ret_status };
  for (int i = 0; i < NUM_DEVICES; ++i) {
    if (g_devices[i].name == name) { 
      ret.device = &g_devices[i];
      ret.ret_status = ESP_OK;
      break;
    }
  }
  return ret;
}

INIT_RET devices_init() {
  esp_err_t ret_status = ESP_FAIL;
  char *name = "none";
  INIT_RET ret = { .dev = NULL, .ret_status = ret_status };

  LED_INIT_RET led_ret = leds_init();
  for (int i = 0; i < NUM_DEVICES; ++i) {
    //Attach LED
    led_t led = { .state = false, .ops = &dev_led_ops }; 
    g_devices[i].led = led;

    g_devices[i].led.ops->set_color(&g_devices[i], PIX_GREEN);
    //Setup GPIO
    ret.ret_status = g_devices[i].ops->setup(&g_devices[i]);

    g_devices[i].led.ops->set_color(&g_devices[i], PIX_BLACK);
  }

  
  return ret;
}

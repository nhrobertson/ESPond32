#include "device.h"
#include "freertos/idf_additions.h"
#include "led.h"
#include "models.h"
#include "esp_log.h"

static const char *TAG = "espond32 - device";


device_t g_devices[NUM_DEVICES] = {
  { .name = "pump1",  .type = DEV_PUMP,  .ops = &o_ops, .pins = &OUTPUT_PINS[0] },
  { .name = "pump2",  .type = DEV_PUMP,  .ops = &o_ops, .pins = &OUTPUT_PINS[1] },
  { .name = "valve1", .type = DEV_VALVE, .ops = &o_ops, .pins = &OUTPUT_PINS[2] },
  { .name = "light1", .type = DEV_LIGHT, .ops = &o_ops, .pins = &OUTPUT_PINS[3] },
  { .name = "float1", .type = DEV_FLOAT, .ops = &i_ops, .pins = &INPUT_PINS }
};

dev_op_state_t resolve_device_state(device_t *dev) {
  dev_op_state_t state = DEV_STATE_ERR;
  switch (dev->type) {
    case DEV_PUMP:
    case DEV_VALVE:
    case DEV_LIGHT:
      switch (dev->u.out.sw) {
        case SW_ON:
          state = DEV_STATE_ON;
          return state;
        case SW_OFF:
          state = DEV_STATE_OFF;
          return state;
        case SW_AUTO:
          state = DEV_STATE_AUTO;
          break;
      }
      
      xSemaphoreTake(ovr_change_mutex, portMAX_DELAY);
      switch (dev->u.out.auto_override) {
        case OVR_ON:
          state = DEV_STATE_ON;
          xSemaphoreGive(ovr_change_mutex);
          return state;
        case OVR_OFF:
          state = DEV_STATE_OFF;
          xSemaphoreGive(ovr_change_mutex);
          return state;
        case OVR_AUTO:
          state = DEV_STATE_AUTO;
          xSemaphoreGive(ovr_change_mutex);
          break;
      }
      
      //Any other forceful state stuff

      return state;
    case DEV_FLOAT:
      return state;
  }
  return state;
}

DEVICE_RET get_device(char* name) {
  esp_err_t ret_status = ESP_ERR_INVALID_ARG;
  device_t *device = NULL;
  DEVICE_RET ret = { .device = device, .ret_status = ret_status };
  for (int i = 0; i < NUM_DEVICES; ++i) {
    if (strcmp(g_devices[i].name, name) == 0) { 
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
    if (ret.ret_status != ESP_OK) {
      ESP_LOGE(TAG, "setup failed for device '%s': %s", g_devices[i].name, esp_err_to_name(ret.ret_status));
    }

    switch (g_devices[i].type) {
      case DEV_PUMP:
      case DEV_VALVE:
      case DEV_LIGHT:
        xSemaphoreTake(ovr_change_mutex, portMAX_DELAY);
        g_devices[i].u.out.auto_override = OVR_AUTO;
        xSemaphoreGive(ovr_change_mutex);
        g_devices[i].u.out.sw = SW_AUTO;
        break;
      case DEV_FLOAT:
        break;
    }

    g_devices[i].led.ops->set_color(&g_devices[i], PIX_BLACK);
  }
  
  ret.ret_status = setup_reset_button();

  ESP_LOGI(TAG, "device init complete: %d devices configured", NUM_DEVICES);

  return ret;
}

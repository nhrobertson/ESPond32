#include <stdio.h>
#include "device.h"
#include "config.h"

device_t g_devices[NUM_DEVICES] = {
  { .name = "pump1",  .kind = DEV_PUMP,  .ops = &output_ops, .pins = &OUTPUT_PINS[0] },
  { .name = "pump2",  .kind = DEV_PUMP,  .ops = &output_ops, .pins = &OUTPUT_PINS[1] },
  { .name = "valve1", .kind = DEV_VALVE, .ops = &output_ops, .pins = &OUTPUT_PINS[2] },
  { .name = "float1", .kind = DEV_FLOAT, .ops = &input_ops,  .pins = &INPUT_PINS }
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
  for (int i = 0; i < NUM_DEVICES; ++i) {
    //Attach LED

  }
}

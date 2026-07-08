#include "tasks.h"
#include "config.h"
#include "device.h"
#include "esp_err.h"
#include "filesystem.h"
#include "float.h"
#include "freertos/idf_additions.h"
#include "freertos/projdefs.h"
#include "led.h"
#include "models.h"
#include "portmacro.h"
#include "date_and_time.h"
#include "esp_log.h"
#include "debug_gpio.h"

static const char *TAG = "espond32 - tasks";

void task_set_sys_light(void *args) {
  bool all_auto;
  while (1) {
    xSemaphoreTake(lockout_change_mutex, portMAX_DELAY);
    bool locked_out = lockout;
    xSemaphoreGive(lockout_change_mutex);
    if (locked_out) {
      set_sys_led_color(PIX_RED);
    } else {
      all_auto = true;
      for (int i = 0; i < NUM_DEVICES; ++i) {
        device_t *dev = &g_devices[i];
        if (dev->type == DEV_FLOAT) { continue; }
        if (resolve_device_state(dev) != DEV_STATE_AUTO) {
          all_auto = false;
          break;
        }
      }
      if (all_auto) {
        set_sys_led_color(PIX_YELLOW);
      } else {
        set_sys_led_color(PIX_BLACK);
      }
    }
    vTaskDelay(pdMS_TO_TICKS(1000));
  }
}

void task_listen_for_task_event(void *args) {
  uint32_t requests = 0;
  while(1) {
    xTaskNotifyWait(0x00,
                    0xFFFFFFFF,
                    &requests,
                    pdMS_TO_TICKS(1000)
        );
    if (requests & REQ_CLEAR_LEAK) {
      clear_leak();
    }
  }
}

void task_check_for_reset(void *args) {
  int level = 0;
  int64_t press_start = 0;
  int64_t now;
  while (1) {
    debug_pin_set(DEBUG_GPIO_RESET, 1);
    level = gpio_get_level(RESET_BTN_GPIO);
    now = esp_timer_get_time();

    if (level == 0) {
      if (press_start == 0) press_start = now;
      else if (now - press_start >= 5LL*1000*1000) {
        clear_leak();
        esp_restart();
      }
    } else {
      press_start = 0;
    }
    debug_pin_set(DEBUG_GPIO_RESET, 0);
    vTaskDelay(pdMS_TO_TICKS(50));
  }
}

void task_check_leak(void *args) {
  bool leak;
  while (1) {
    debug_pin_set(DEBUG_GPIO_LEAK, 1);
    leak = check_leak_check();
    if (leak) {
      xSemaphoreTake(lockout_change_mutex, portMAX_DELAY);
      lockout = true;
      xSemaphoreGive(lockout_change_mutex);
      set_sys_led_color(PIX_RED);
      nvs_set_lockout(1);
      xEventGroupSetBits(s_net_events, STATUS_CHANGE_BIT);
    }
    check_fills_per_day();
    debug_pin_set(DEBUG_GPIO_LEAK, 0);
    vTaskDelay(pdMS_TO_TICKS(10));
  }
}

void task_check_io(void *args) {
  //Iterate over all devices
  esp_err_t err_check;

  while (1) {
    debug_pin_set(DEBUG_GPIO_IO, 1);
    for (int i = 0; i < NUM_DEVICES; ++i) {
      device_t *self = &g_devices[i];
      err_check = self->ops->check(self);
      if (err_check != ESP_OK) {
        ESP_LOGW(TAG, "check failed for device '%s': %s", self->name, esp_err_to_name(err_check));
      }
    }
    debug_pin_set(DEBUG_GPIO_IO, 0);
    vTaskDelay(pdMS_TO_TICKS(10));
  }
}

void task_check_cfg(void *args) {
 while (1) {
   EventBits_t bits = xEventGroupWaitBits(g_events, CFG_CHANGED_BIT, pdTRUE, pdFALSE, portMAX_DELAY);
   if (bits & CFG_CHANGED_BIT) {
     xSemaphoreTake(cfg_buff_mutex, portMAX_DELAY);
     //The CFG has changed begin update process
     nvs_store_espond_cfg(&g_buff_cfg);
     xSemaphoreTake(cfg_change_mutex, portMAX_DELAY);
     g_espond_cfg = g_buff_cfg;
     xSemaphoreGive(cfg_change_mutex);
     xSemaphoreGive(cfg_buff_mutex);
   }
 } 
}

void task_evaluate_cfg(void *args) {
  esp_err_t err_check;
  bool refill = false;

  while (1) {
    debug_pin_set(DEBUG_GPIO_EVAL, 1);
    for (int i = 0; i < NUM_DEVICES; ++i) {
      device_t *self = &g_devices[i];
      switch (self->type) {
        case DEV_PUMP:
        case DEV_VALVE:
          //Lockout due to leak detected
          xSemaphoreTake(lockout_change_mutex, portMAX_DELAY);
          if (lockout) {
            self->u.out.state = DEV_STATE_OFF;
            xSemaphoreGive(lockout_change_mutex);
            break;
          }
          xSemaphoreGive(lockout_change_mutex);
        case DEV_LIGHT:
          dev_op_state_t state = resolve_device_state(self);
          
          if (state == DEV_STATE_AUTO) {
            state = evaluate_device_schedule(self);
          }
          
          self->u.out.state = state;
          
          break;
        case DEV_FLOAT:
          bool check;
          check = eval_float_state(self);
          if (check) {
            DEVICE_RET dev_ret = get_device("valve1");
            xSemaphoreTake(ovr_change_mutex, portMAX_DELAY);
            if (dev_ret.ret_status == ESP_OK) {
              if (dev_ret.device->type == DEV_VALVE && 
                  dev_ret.device->u.out.auto_override != OVR_OFF) {
                
                device_t *dev = dev_ret.device;
                dev->u.out.auto_override = OVR_ON;
                refill = true;
              }
            }
            xSemaphoreGive(ovr_change_mutex);
          } else if (refill) {
            DEVICE_RET dev_ret = get_device("valve1");
            if (dev_ret.ret_status == ESP_OK) {
              xSemaphoreTake(ovr_change_mutex, portMAX_DELAY);
              if (dev_ret.device->u.out.auto_override == OVR_ON) {
                dev_ret.device->u.out.auto_override = OVR_AUTO;
              }
              xSemaphoreGive(ovr_change_mutex);
              refill = false;
            }
          }

          break;
      }

    }
    debug_pin_set(DEBUG_GPIO_EVAL, 0);
    vTaskDelay(pdMS_TO_TICKS(250));
  }
}

void task_operate(void *args) {
  esp_err_t err_check;
  //Iterate over all device
  while (1) {
    debug_pin_set(DEBUG_GPIO_OPERATE, 1);
    for (int i = 0; i < NUM_DEVICES; ++i) {
      device_t *self = &g_devices[i];

      switch (self->type) {
        case DEV_PUMP:
        case DEV_VALVE:
        case DEV_LIGHT:
          switch (self->u.out.state) {
            io_mode_t control;
            case DEV_STATE_ON:
              if (self->u.out.on) { break; }
              control.state = GPIO_ON;
              control.value = 1;
              err_check = self->ops->operate(self, &control);
              if (err_check != ESP_OK) {
                ESP_LOGE(TAG, "failed to turn ON device '%s': %s", self->name, esp_err_to_name(err_check));
                break;
              }
              self->u.out.on = true;
              self->led.ops->set_color(self, PIX_GREEN);
              xEventGroupSetBits(s_net_events, STATUS_CHANGE_BIT);
              break;
            case DEV_STATE_OFF:
              if (!self->u.out.on) { break; }
              control.state = GPIO_OFF; 
              control.value = 0;
              err_check = self->ops->operate(self, &control);
              if (err_check != ESP_OK) {
                ESP_LOGE(TAG, "failed to turn OFF device '%s': %s", self->name, esp_err_to_name(err_check));
                break;
              }
              self->u.out.on = false;
              self->led.ops->set_color(self, PIX_BLACK);
              xEventGroupSetBits(s_net_events, STATUS_CHANGE_BIT);
              break;
            default:
              //Device state dev_op_state_t should only ever be on/off
              err_check = ESP_ERR_INVALID_ARG;
              break;
          }
          break;
        case DEV_FLOAT:
          if (self->u.in.active) {
            self->led.ops->set_color(self, PIX_BLUE);
          } else {
            self->led.ops->set_color(self, PIX_BLACK);
          }
          break;
      }
    }

    debug_pin_set(DEBUG_GPIO_OPERATE, 0);
    vTaskDelay(pdMS_TO_TICKS(50));
  }
}

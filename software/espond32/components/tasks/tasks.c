#include "tasks.h"
#include "freertos/idf_additions.h"
#include "freertos/projdefs.h"
#include "models.h"
#include "portmacro.h"

void task_check_io(void *args) {
  //Iterate over all devices
  esp_err_t err_check;
  
  while (1) {
    for (int i = 0; i < NUM_DEVICES; ++i) {
      device_t *self = &g_devices[i];

      err_check = self->ops->check(self);
      if (err_check != ESP_OK) {
        // Handle Error
      }
    
    }
    vTaskDelay(pdMS_TO_TICKS(50));
  }
}

void tasks_check_cfg(void *args) {
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

void task_operate(void *args) {
  esp_err_t err_check;
  //Iterate over all device
  while (1) {
    for (int i = 0; i < NUM_DEVICES; ++i) {
      device_t *self = &g_devices[i];

      switch (self->type) {
        case DEV_PUMP:
        case DEV_VALVE:
        case DEV_LIGHT:
          switch (self->u.out.sw) {
            io_mode_t control;
            case SW_ON:
              control.state = GPIO_ON;
              control.value = 1;
              err_check = self->ops->operate(self, &control);
              break;
            case SW_OFF:
              control.state = GPIO_OFF; 
              control.value = 0;
              err_check = self->ops->operate(self, &control);
              break;
            case SW_AUTO:
              //Get current time

              //Compare to CFG
              
              //Act Accordingly
              
              break;
          }
          break;
        case DEV_FLOAT:
          io_mode_t state;
          err_check = self->ops->operate(self, &state);
          if (state.value > 0) {
            //Recieved Signal, check / start timer
            
            //If timer > FLOAT_TIMER_TIME, enable valve
          }
          break;
      }
    }

    vTaskDelay(pdMS_TO_TICKS(50));
  }
}

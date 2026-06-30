#include "tasks.h"
#include "freertos/idf_additions.h"
#include "freertos/projdefs.h"
#include "models.h"

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
          switch (self->u.out.sw) {
            io_mode_t control;
            case SW_ON:
              control.state = GPIO_ON;
              control.value = 1;
              err_check = self->ops->operate(self, &control);
              break;
            case SW_OFF:
              control.state = GPIO_OFF; 
              control.value = 0;;
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

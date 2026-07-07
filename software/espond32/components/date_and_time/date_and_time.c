#include "date_and_time.h"
#include "models.h"
#include "portmacro.h"

struct tm get_local_time(void)
{
  time_t now;
  struct tm timeinfo;

  time(&now);
  localtime_r(&now, &timeinfo);

  return timeinfo;
}


dev_op_state_t evaluate_device_schedule(device_t *dev) {
  dev_op_state_t state = DEV_STATE_OFF;
  struct tm time = get_local_time();
  
  output_schedule_t schedule;
  //Grab Device
  for (int i = 0; i < NUM_OUTPUTS; ++i) {
    volatile char* comp_name = g_espond_cfg.outputs[i].name;
    
    if (strcmp(dev->name, (const char*)comp_name) == 0) {
      xSemaphoreTake(cfg_change_mutex, portMAX_DELAY);
      schedule = g_espond_cfg.outputs[i];
      xSemaphoreGive(cfg_change_mutex);
      break;
    }
  }
  
  int now_min = time.tm_hour * 60 + time.tm_min;
  int on_min = schedule.on_hour * 60 + schedule.on_min;
  int off_min = schedule.off_hour * 60 + schedule.off_min;

  if ((schedule.days_mask & (1 << time.tm_wday))) { 
    if (on_min <= off_min) {
      if (now_min >= on_min && now_min < off_min) {
        state = DEV_STATE_ON;
      } 
    } else {
      if (now_min >= on_min || now_min < off_min) {
        state = DEV_STATE_ON;
      }
    }
  }

  return state;
}

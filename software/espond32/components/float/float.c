#include "float.h"
#include "models.h"
#include "portmacro.h"
#include "esp_log.h"

static const char *TAG = "espond32 - float";

static int64_t float_deadline_us = 0;
static leak_check_t leak_check = {0};

static inline int64_t minutes_us(int m) {
  return (int64_t)m * 60 * 1000000;
}

void clear_leak() {
  xSemaphoreTake(leak_check_mutex, portMAX_DELAY);
  leak_check.count = 0;
  xSemaphoreGive(leak_check_mutex);
  xSemaphoreTake(lockout_change_mutex, portMAX_DELAY);
  lockout = false;
  xSemaphoreGive(lockout_change_mutex);
  set_sys_led_color(PIX_BLACK);
  nvs_clear_lockout();
}

bool check_leak_check() {
  xSemaphoreTake(cfg_change_mutex, portMAX_DELAY);
  int max_fills = g_espond_cfg.float_sens.max_fills_per_day;
  xSemaphoreGive(cfg_change_mutex);

  xSemaphoreTake(leak_check_mutex, portMAX_DELAY);
  bool over = leak_check.count > max_fills;
  xSemaphoreGive(leak_check_mutex);
  return over;
}

bool check_fills_per_day() {
  struct tm now = get_local_time();

  xSemaphoreTake(leak_check_mutex, portMAX_DELAY);
  bool same_day = leak_check.d == now.tm_wday;
  if (!same_day) {
    leak_check.count = 0;
    leak_check.d = now.tm_wday;
  }
  xSemaphoreGive(leak_check_mutex);
  return same_day;
}

bool eval_float_state(device_t *dev) {
  int64_t time_us = esp_timer_get_time();
  
  bool active = dev->u.in.active;
  switch (dev->u.in.state) {
    case (FILL_IDLE):
      if (active) {
        xSemaphoreTake(cfg_change_mutex, portMAX_DELAY);
        int threshold_min = g_espond_cfg.float_sens.threshold_min;
        xSemaphoreGive(cfg_change_mutex);
        float_deadline_us = time_us + minutes_us(threshold_min);
        dev->u.in.state = FILL_ARMING;
        ESP_LOGI(TAG, "float active, arming refill");
      }
      return false;
    case (FILL_ARMING):
      if (!active) { dev->u.in.state = FILL_IDLE; return false; }
      if (time_us >= float_deadline_us) {
        dev->u.in.state = FILL_FILLING;
        ESP_LOGI(TAG, "refill threshold met, filling");
        return true;
      }
      return false;
    case (FILL_FILLING):
      if (!active) {
        xSemaphoreTake(cfg_change_mutex, portMAX_DELAY);
        int overflow_min = g_espond_cfg.float_sens.overflow_min;
        xSemaphoreGive(cfg_change_mutex);
        float_deadline_us = time_us + minutes_us(overflow_min);
        dev->u.in.state = FILL_OVERFLOW;
        ESP_LOGI(TAG, "float cleared, holding fill for overflow window");
      }
      return true;

    case (FILL_OVERFLOW):
      if (active) { dev->u.in.state = FILL_FILLING; return true; }
      if (time_us >= float_deadline_us) {
        dev->u.in.state = FILL_IDLE;
        xSemaphoreTake(leak_check_mutex, portMAX_DELAY);
        leak_check.count++;
        int fill_count = leak_check.count;
        xSemaphoreGive(leak_check_mutex);
        ESP_LOGI(TAG, "fill cycle complete (count today: %d)", fill_count);
        return false;
      }
      return true;
  }
  return false;
}

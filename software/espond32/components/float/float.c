#include "float.h"

static int64_t float_deadline_us = 0;
static leak_check_t leak_check = {0};

static inline int64_t minutes_us(int m) {
  return (int64_t)m * 60 * 1000000;
}

void clear_leak() {
  leak_check.count = 0;
}

bool check_leak_check() {
  if (leak_check.count > g_espond_cfg.float_sens.max_fills_per_day) {
    return true;
  }
  return false;
}

bool check_fills_per_day() {
  struct tm now = get_local_time();

  if (leak_check.d != now.tm_wday) {
    leak_check.count = 0;
    leak_check.d = now.tm_wday;
    return false;
  }
  return true;
}

bool eval_float_state(device_t *dev) {
  int64_t time_us = esp_timer_get_time();
  
  bool active = dev->u.in.active;
  switch (dev->u.in.state) {
    case (FILL_IDLE):
      if (active) {
        float_deadline_us = time_us;
        dev->u.in.state = FILL_ARMING;
      } 
      return false;
    case (FILL_ARMING):
      if (!active) { dev->u.in.state = FILL_IDLE; return false; }
      if (time_us >= float_deadline_us) {
        dev->u.in.state = FILL_FILLING;
        return true;
      }
      return false;
    case (FILL_FILLING):
      if (!active) {
        float_deadline_us = time_us + minutes_us(g_espond_cfg.float_sens.overflow_min);
        dev->u.in.state = FILL_OVERFLOW;
      }
      return true;

    case (FILL_OVERFLOW):
      if (active) { dev->u.in.state = FILL_FILLING; return true; }
      if (time_us >= float_deadline_us) {
        dev->u.in.state = FILL_IDLE;
        leak_check.count++;
        return false;
      }
      return true;
  }
  return false;
}

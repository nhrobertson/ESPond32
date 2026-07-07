#include "models.h"
#include "config.h"
#include "freertos/idf_additions.h"
#include <string.h>

volatile espond_cfg_t g_espond_cfg = {0};
espond_cfg_t g_buff_cfg   = {0};

EventGroupHandle_t g_events;
SemaphoreHandle_t cfg_buff_mutex;
SemaphoreHandle_t cfg_change_mutex;
TaskHandle_t operate_handle;

// "HH:MM" -> hour/min, with range checking. Returns false if malformed.
static bool parse_hhmm(const cJSON *item, uint8_t *h, uint8_t *m) {
    if (!cJSON_IsString(item) || item->valuestring == NULL) return false;
    int hh, mm;
    if (sscanf(item->valuestring, "%d:%d", &hh, &mm) != 2) return false;
    if (hh < 0 || hh > 23 || mm < 0 || mm > 59)            return false;
    *h = (uint8_t)hh; *m = (uint8_t)mm;
    return true;
}

/**
 *  Json Format:
 *  {
 *    "version": X,
 *    "outputs": [
 *    { "name": "pump1", "on": "XX:XX", "off": "XX:XX", "days": [0-6,x7] },
 *    { "name": "pump2", "on": "XX:XX", "off": "XX:XX", "days": [0-6,x7] },
 *    { "name": "valve1", "on": "XX:XX", "off": "XX:XX", "days": [0-6,x7] },
 *    { "name": "lights1", "on": "XX:XX", "off": "XX:XX", "days": [0-6,x7] },
 *    ],
 *    "float_sens": { "threshold_min": 10, "overflow_min": 30, "max_fills_per_day": 3}
 *  }
 *  
 */
esp_err_t parse_config_json(const char *config_str, espond_cfg_t *out) {
  esp_err_t ret = ESP_ERR_INVALID_RESPONSE;
  cJSON *root = cJSON_Parse(config_str);
  if (root == NULL) {
    return ret;
  }

  espond_cfg_t cfg = {0};

  const cJSON *version = cJSON_GetObjectItem(root, "version");
  if (!cJSON_IsNumber(version)) goto cleanup;
  cfg.version = version->valueint;
  
  const cJSON *outputs = cJSON_GetObjectItem(root, "outputs");
  if (!cJSON_IsArray(outputs)) goto cleanup;
  
  const cJSON *o = NULL;
  cJSON_ArrayForEach(o, outputs) {
    if (cfg.num_outputs >= NUM_OUTPUTS) goto cleanup;
    output_schedule_t *dst = &cfg.outputs[cfg.num_outputs];
    
    const cJSON *name = cJSON_GetObjectItem(o, "name");
    if (!cJSON_IsString(name)) goto cleanup;
    strlcpy(dst->name, name->valuestring, sizeof(dst->name));

    const cJSON *on = cJSON_GetObjectItem(o, "on");
    const cJSON *off = cJSON_GetObjectItem(o, "off");
    if (cJSON_IsNull(on) && cJSON_IsNull(off)) {
      dst->has_schedule = false;
    } else if (
        parse_hhmm(on, &dst->on_hour, &dst->on_min) &&
        parse_hhmm(off, &dst->off_hour, &dst->off_min)
        ) {
      dst->has_schedule = true;
    } else {
      goto cleanup;
    }

    const cJSON *days = cJSON_GetObjectItem(o, "days");
    if (!cJSON_IsArray(days)) goto cleanup;
    const cJSON *d = NULL;
    cJSON_ArrayForEach(d, days) {
      if (!cJSON_IsNumber(d)) goto cleanup;
      dst->days_mask |= (uint8_t)(1 << d->valueint);
    }

    cfg.num_outputs++;
  }

  const cJSON *fs = cJSON_GetObjectItem(root, "float_sens");
  if (!cJSON_IsObject(fs)) goto cleanup;
  const cJSON *thr = cJSON_GetObjectItem(fs, "threshold_min");
  const cJSON *ovf = cJSON_GetObjectItem(fs, "overflow_min");
  const cJSON *mfd = cJSON_GetObjectItem(fs, "max_fills_per_day");
  if (!cJSON_IsNumber(thr) || !cJSON_IsNumber(ovf) || !cJSON_IsNumber(mfd)) goto cleanup;
  cfg.float_sens.threshold_min = thr->valueint;
  cfg.float_sens.overflow_min = ovf->valueint;
  cfg.float_sens.max_fills_per_day = mfd->valueint;
  
  *out = cfg;
  ret = ESP_OK;

cleanup:
  cJSON_Delete(root);
  return ret;
}

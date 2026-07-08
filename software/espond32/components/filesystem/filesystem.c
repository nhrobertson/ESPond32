#include "filesystem.h"
#include "nvs.h"
#include "esp_log.h"

static const char *TAG = "espond32 - filesystem";

espond_cfg_t cfg_default() {
  espond_cfg_t cfg = {
    .version = 0,
    .num_outputs = NUM_OUTPUTS,
    .outputs = {
      { .name = "pump1", .on_hour = 12, .on_min = 0, .off_hour = 12, .off_min = 1, .days_mask = 0b1111111 }, 
      { .name = "pump2", .on_hour = 12, .on_min = 0, .off_hour = 12, .off_min = 1, .days_mask = 0b1111111 }, 
      { .name = "valve1", .on_hour = 12, .on_min = 0, .off_hour = 12, .off_min = 1, .days_mask = 0b1111111 }, 
      { .name = "light1", .on_hour = 12, .on_min = 0, .off_hour = 12, .off_min = 1, .days_mask = 0b1111111 }, 
    }, 
    .float_sens = {
      .threshold_min = 5,
      .overflow_min  = 5,
      .max_fills_per_day = 3,
    },
  };

  return cfg;
}


esp_err_t nvs_load_espond_cfg(espond_cfg_t *cfg) {
  nvs_handle_t handle;
  esp_err_t ret;

  ret = nvs_open(NVS_NAMESPACE, NVS_READONLY, &handle);
  if (ret != ESP_OK) return ret;

  size_t need = 0;
  ret = nvs_get_blob(handle, NVS_CFG_KEY, NULL, &need);
  if (ret == ESP_OK && need != sizeof(*cfg)) {
    ret = ESP_ERR_INVALID_SIZE;
  }
  if (ret == ESP_OK) {
    ret = nvs_get_blob(handle, NVS_CFG_KEY, cfg, &need);
  }
  nvs_close(handle);

  return ret;
}

esp_err_t nvs_store_espond_cfg(espond_cfg_t *cfg) {
  nvs_handle_t handle;
  esp_err_t ret;

  ret = nvs_open(NVS_NAMESPACE, NVS_READWRITE, &handle);
  if (ret != ESP_OK) return ret;

  ret = nvs_set_blob(handle, NVS_CFG_KEY, cfg, sizeof(*cfg));
  if (ret == ESP_OK) ret = nvs_commit(handle);
  nvs_close(handle);
  return ret;
}

void cfg_load(espond_cfg_t *cfg) {
  esp_err_t ret;

  ret = nvs_load_espond_cfg(cfg);
  if (ret != ESP_OK) {
    ESP_LOGW(TAG, "no valid config in NVS (%s), loading defaults", esp_err_to_name(ret));
    *cfg = cfg_default();
    nvs_store_espond_cfg(cfg);
  } else {
    ESP_LOGI(TAG, "loaded config from NVS, version %d", cfg->version);
  }
}

esp_err_t nvs_clear_lockout() {
  nvs_handle_t handle;
  esp_err_t err = nvs_open("leak", NVS_READWRITE, &handle);
  if (err != ESP_OK) {
    ESP_LOGE(TAG, "nvs_clear_lockout: nvs_open failed: %s", esp_err_to_name(err));
  }

  nvs_set_u8(handle, "lockout", 0);

  err = nvs_commit(handle);
  nvs_close(handle);
  return err;
}

esp_err_t nvs_set_lockout(uint8_t lockout) {
  nvs_handle_t handle;
  esp_err_t err = nvs_open("leak", NVS_READWRITE, &handle);
  if (err != ESP_OK) {
    ESP_LOGE(TAG, "nvs_set_lockout: nvs_open failed: %s", esp_err_to_name(err));
  }

  nvs_set_u8(handle, "lockout", lockout);

  err = nvs_commit(handle);
  nvs_close(handle);
  return err;
}

esp_err_t nvs_load_lockout(bool *lockout) {
 
  nvs_handle_t handle;
  esp_err_t err = nvs_open("leak", NVS_READWRITE, &handle);
  if (err != ESP_OK) {
    ESP_LOGE(TAG, "nvs_load_lockout: nvs_open failed: %s", esp_err_to_name(err));
  }

  uint8_t lockout_u8 = 0;

  nvs_get_u8(handle, "lockout", &lockout_u8);

  if (lockout_u8) {
    *lockout = true;
  } else {
    *lockout = false;
  }

  nvs_close(handle);
  return err;
}

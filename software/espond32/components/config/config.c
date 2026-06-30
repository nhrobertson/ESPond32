#include "config.h"

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
esp_err_t parse_config_json(char *config_str) {
  cJSON *root = cJSON_Parse(config_str);
  if (root == NULL) {
    const char *error_ptr = cJSON_GetErrorPtr();
    if (error_ptr != NULL) {
      return ESP_ERR_INVALID_RESPONSE;
    }
  

  }
}

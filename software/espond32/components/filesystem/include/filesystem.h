#ifndef FILESYSTEM_H
#define FILESYSTEM_H

#include "models.h"
#include "nvs_flash.h"

#define NVS_NAMESPACE "pond"
#define NVS_CFG_KEY   "cfg"

esp_err_t nvs_store_espond_cfg(espond_cfg_t *cfg);
void cfg_load(espond_cfg_t *cfg);

#endif // !FILESYSTEM_H

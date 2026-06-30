#include "network.h"
#include "freertos/idf_additions.h"
#include "freertos/projdefs.h"

static EventGroupHandle_t s_net_events;

static void task_net_manager(void *arg) {
  
  //Begin network initalization
  esp_netif_init();
  esp_event_loop_create_default();
  esp_netif_create_default_wifi_sta();

  wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT(); //TODO: Setup
  esp_wifi_init(&cfg);
  register_wifi_and_ip_handlers();

  esp_wifi_set_mode(WIFI_MODE_STA);
  esp_wifi_set_config(WIFI_IF_STA, &sta_cfg);
  esp_wifi_start();
  esp_wifi_connect();

  bool services_up = false;
  while (1) {
    EventBits_t bits = xEventGroupWaitBits(s_net_events, GOT_IP_BIT | DISCONNECT_BIT, pdTRUE, pdFALSE, portMAX_DELAY);

    if (bits & GOT_IP_BIT) {
      start_sntp();
      start_mqtt();
      services_up = true;
    }
    if (bits & DISCONNECT_BIT) {
      if (services_up) stop_mqtt();
      services_up = false;
      esp_wifi_connect();
    }
    vTaskDelay(pdMS_TO_TICKS(100));
  }
}

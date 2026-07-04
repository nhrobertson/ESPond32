#include "network.h"

static EventGroupHandle_t s_net_events;
static bool s_mqtt_running = false;

wifi_config_t sta_cfg = {
  .sta = {
    .ssid       = "ssid",
    .password   = "password",
    .threshold.authmode = WIFI_AUTH_WPA2_PSK,
  }
};

static void start_mqtt(void) {
  if (s_mqtt_running) return;
  esp_mqtt_client_start();
  s_mqtt_running = true;
}

static void start_sntp(void) {
  esp_sntp_config_t config = ESP_NETIF_SNTP_DEFAULT_CONFIG("pool.ntp.org");
  esp_netif_sntp_init(&config); //Auto Syncs Time
}

static void stop_mqtt(void) {
  if (!s_mqtt_running) return;
  esp_mqtt_client_stop();
  s_mqtt_running = false;
}

static void wifi_event_handler(void *arg, esp_event_base_t base, int32_t id, void *data) {
  if (base == WIFI_EVENT && id == WIFI_EVENT_STA_START) {
    esp_wifi_connect();
  } else if (base == WIFI_EVENT && id == WIFI_EVENT_STA_DISCONNECTED) {
    xEventGroupSetBits(s_net_events, DISCONNECT_BIT);
  } else if (base == IP_EVENT && id == IP_EVENT_STA_GOT_IP) {
    xEventGroupSetBits(s_net_events, GOT_IP_BIT);
  }
}

static void register_network_handlers(void) {
  esp_event_handler_instance_register(WIFI_EVENT, ESP_EVENT_ANY_ID, 
      &wifi_event_handler, NULL, NULL);

  esp_event_handler_instance_register(IP_EVENT, IP_EVENT_STA_GOT_IP, 
      &wifi_event_handler, NULL, NULL);
}


static void task_net_manager(void *arg) {
  //Begin network initalization
  esp_netif_init();
  esp_event_loop_create_default();
  esp_netif_create_default_wifi_sta();

  wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
  esp_wifi_init(&cfg);
  register_network_handlers();

  esp_wifi_set_mode(WIFI_MODE_STA);
  esp_wifi_set_config(WIFI_IF_STA, &sta_cfg);
  esp_wifi_start();

  bool services_up = false;
  int backoff_ms = 1000;
  while (1) {
    EventBits_t bits = xEventGroupWaitBits(s_net_events, GOT_IP_BIT | DISCONNECT_BIT, pdTRUE, pdFALSE, portMAX_DELAY);

    if (bits & GOT_IP_BIT) {
      start_sntp();
      start_mqtt();
      services_up = true;
    }
    if (bits & DISCONNECT_BIT) {
      if (services_up) {
        stop_mqtt();
        services_up = false;
        vTaskDelay(pdMS_TO_TICKS(backoff_ms));
        if (backoff_ms < 30000) backoff_ms *= 2;
      }
      esp_wifi_connect();
    }
    vTaskDelay(pdMS_TO_TICKS(100));
  }
}

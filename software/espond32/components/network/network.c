#include "network.h"
#include "config.h"
#include "esp_system.h"
#include "freertos/idf_additions.h"
#include "mqtt_client.h"
#include "portmacro.h"
#include "models.h"
#include "esp_log.h"
#include "debug_gpio.h"
#include <stdio.h>
#include <string.h>

static const char *TAG = "espond32 - network";

EventGroupHandle_t s_net_events;
static esp_mqtt_client_handle_t s_mqtt_client;
static bool s_mqtt_running = false;
static bool s_sntp_running = false;


wifi_config_t sta_cfg = {
  .sta = {
    .ssid       = "ssid",
    .password   = "password",
    .threshold.authmode = WIFI_AUTH_WPA2_PSK,
  }
};

esp_mqtt_client_config_t mqtt_cfg = {
    .broker = {
        .address.uri = "mqtt://IP_ADDR",   // your Mosquitto box
    },
    .credentials = {
        .client_id = "espondcu",            // stable, human-readable
        .username  = "espond",                          // if the broker requires auth
        .authentication.password = "password",
    },
    .session = {
        .last_will = {
            .topic  = "pond/availability",
            .msg    = "offline",
            .qos    = 1,
            .retain = true,
        },
        .keepalive = 30,                              // seconds
    },
    .network = {
        .reconnect_timeout_ms = 10000,               // wait between auto-retries
        .timeout_ms           = 10000,               // network op timeout
    },  
};

static void start_mqtt(esp_mqtt_client_handle_t client) {
  if (s_mqtt_running) return;
  ESP_LOGI(TAG, "starting MQTT client");
  esp_mqtt_client_start(client);
  s_mqtt_running = true;
}

static void start_sntp(void) {
  if (s_sntp_running) return;
  esp_sntp_config_t config = ESP_NETIF_SNTP_DEFAULT_CONFIG("pool.ntp.org");
  esp_netif_sntp_init(&config); //Auto Syncs Time
  setenv("TZ", "EST5EDT,M3.2.0,M11.1.0", 1);
  tzset();
  s_sntp_running = true;
}

static void stop_mqtt(esp_mqtt_client_handle_t client) {
  if (!s_mqtt_running) return;
  ESP_LOGI(TAG, "stopping MQTT client");
  esp_mqtt_client_stop(client);
  s_mqtt_running = false;
}

static void wifi_event_handler(void *arg, esp_event_base_t base, int32_t id, void *data) {
  if (base == WIFI_EVENT && id == WIFI_EVENT_STA_START) {
    esp_wifi_connect();
  } else if (base == WIFI_EVENT && id == WIFI_EVENT_STA_DISCONNECTED) {
    ESP_LOGW(TAG, "wifi disconnected");
    xEventGroupSetBits(s_net_events, DISCONNECT_BIT);
  } else if (base == IP_EVENT && id == IP_EVENT_STA_GOT_IP) {
    ESP_LOGI(TAG, "wifi connected, got IP");
    xEventGroupSetBits(s_net_events, GOT_IP_BIT);
  }
}

static void register_network_handlers(void) {
  esp_event_handler_instance_register(WIFI_EVENT, ESP_EVENT_ANY_ID, 
      &wifi_event_handler, NULL, NULL);

  esp_event_handler_instance_register(IP_EVENT, IP_EVENT_STA_GOT_IP, 
      &wifi_event_handler, NULL, NULL);
}

static void handle_config_msg(esp_mqtt_event_handle_t event) {
  char *temp_json = malloc(event->data_len + 1);
  if (temp_json == NULL) return;
  memcpy(temp_json, event->data, event->data_len);
  temp_json[event->data_len] = '\0';

  espond_cfg_t recieved;
  esp_err_t err = parse_config_json(temp_json, &recieved);
  free(temp_json);

  if (err != ESP_OK) {
    ESP_LOGW(TAG, "received malformed config message: %s", esp_err_to_name(err));
    return;
  }

  if (recieved.version > g_espond_cfg.version) {
    ESP_LOGI(TAG, "applying config update, version %d -> %d", g_espond_cfg.version, recieved.version);
    xSemaphoreTake(cfg_buff_mutex, portMAX_DELAY);
    //Begin Replacement
    g_buff_cfg = recieved;
    xSemaphoreGive(cfg_buff_mutex);
    xEventGroupSetBits(g_events, CFG_CHANGED_BIT);
  }
}

static void handle_command_msg(esp_mqtt_event_handle_t event) {
  char *cmd = malloc(event->data_len + 1);
  if (!cmd) return;
  memcpy(cmd, event->data, event->data_len);
  cmd[event->data_len] = '\0';

  if (strcmp(cmd, "clear_leak_lockout") == 0) {
    ESP_LOGI(TAG, "command received: clear_leak_lockout");
    xTaskNotify(task_handle, REQ_CLEAR_LEAK, eSetBits);
  } else if (strcmp(cmd, "reboot") == 0) {
    ESP_LOGI(TAG, "command received: reboot");
    free(cmd);
    esp_restart();
  } else if (strcmp(cmd, "request_status") == 0) {
    xEventGroupSetBits(s_net_events, STATUS_CHANGE_BIT);
  } else {
    ESP_LOGW(TAG, "unknown command received: %s", cmd);
  }
  free(cmd);
}

static void handle_override_msg(esp_mqtt_event_handle_t event) {
  char *json = malloc(event->data_len + 1);
  if (!json) return;
  memcpy(json, event->data, event->data_len);
  json[event->data_len] = '\0';

  override_json_t recieved;
  esp_err_t err = parse_override_json(json, &recieved);
  free(json);

  if (err != ESP_OK) {
    ESP_LOGW(TAG, "received malformed override message: %s", esp_err_to_name(err));
    return;
  }

  DEVICE_RET device_ret = get_device(recieved.name);
  if (device_ret.ret_status != ESP_OK) {
    ESP_LOGW(TAG, "override for unknown device '%s'", recieved.name);
    return;
  }
  if (device_ret.device->type == DEV_FLOAT) return;
  ESP_LOGI(TAG, "override received: '%s' -> %d", recieved.name, recieved.override);
  xSemaphoreTake(ovr_change_mutex, portMAX_DELAY);
  device_ret.device->u.out.auto_override = recieved.override;
  xSemaphoreGive(ovr_change_mutex);
}

static void publish_status(esp_mqtt_client_handle_t client) {
  char buf[512];
  int n = 0;

  n += sniprintf(buf + n, sizeof(buf) - n,
      "{\"version\":%d,\"leak_lockout\":%s,\"outputs\":[",
      g_espond_cfg.version,
      lockout ? "true" : "false"
      );


  status_json_builder_t devices[NUM_DEVICES] = {0};
  for (int i = 0; i < NUM_DEVICES; ++i) {
    switch (g_devices[i].type) {
      case DEV_VALVE:
      case DEV_PUMP:
      case DEV_LIGHT:
        strlcpy(devices[i].name, g_devices[i].name, sizeof(devices[i].name));
        devices[i].state = g_devices[i].u.out.on;
        break;
      case DEV_FLOAT:
        strlcpy(devices[i].name, g_devices[i].name, sizeof(devices[i].name));
        devices[i].state = g_devices[i].u.in.active;
        break;
    }

    n += sniprintf(buf + n, sizeof(buf) - n,
        "%s{\"name\":\"%s\",\"state\":%s}",
        (i > 0) ? "," : "",
        devices[i].name,
        devices[i].state ? "true" : "false"
        );
  }

  n += snprintf(buf + n, sizeof(buf) - n, "]}");

  esp_mqtt_client_publish(client, "pond/status", buf, 0, 1, true);
}

static void mqtt_event_handler(void *arg, esp_event_base_t base, int32_t event_id, void *event_data) {
  esp_mqtt_event_handle_t event = event_data;

  switch((esp_mqtt_event_id_t)event_id) {
    case MQTT_EVENT_CONNECTED:
      ESP_LOGI(TAG, "MQTT connected to broker");
      esp_mqtt_client_publish(event->client, "pond/availability", "online", 0, 1, true);

      esp_mqtt_client_subscribe(event->client, "pond/config", 1);
      esp_mqtt_client_subscribe(event->client, "pond/cmd", 1);
      esp_mqtt_client_subscribe(event->client, "pond/override", 1);
      break;
    case MQTT_EVENT_DATA:
      if (strncmp(event->topic, "pond/config", event->topic_len) == 0) {
        //Handle Config Message
        handle_config_msg(event);
      } else if (strncmp(event->topic, "pond/cmd", event->topic_len) == 0) {
        //Handle Command Message
        handle_command_msg(event);
      } else if (strncmp(event->topic, "pond/override", event->topic_len) == 0) {
        //Handle Override Message
        handle_override_msg(event);
      }
      break;
    case MQTT_EVENT_ERROR:
      ESP_LOGE(TAG, "MQTT error, type=%d", event->error_handle->error_type);
      break;
    default:
      break;
  }
}


void task_net_manager(void *arg) {
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

  s_mqtt_client = esp_mqtt_client_init(&mqtt_cfg);
  esp_mqtt_client_register_event(s_mqtt_client, ESP_EVENT_ANY_ID, mqtt_event_handler, NULL);

  bool services_up = false;
  int backoff_ms = 1000;
  while (1) {
    debug_pin_set(DEBUG_GPIO_NET, 0);
    EventBits_t bits = xEventGroupWaitBits(s_net_events, GOT_IP_BIT | DISCONNECT_BIT | STATUS_CHANGE_BIT, pdTRUE, pdFALSE, portMAX_DELAY);
#if DEBUG_INDEPTH_LOG
    ESP_LOGD(TAG, "task_net_manager");
#endif
    debug_pin_set(DEBUG_GPIO_NET, 1);

    if (bits & GOT_IP_BIT) {
      backoff_ms = 1000;
      start_sntp();
      start_mqtt(s_mqtt_client);
      services_up = true;
    }
    if (bits & DISCONNECT_BIT) {
      if (services_up) {
        stop_mqtt(s_mqtt_client);
        services_up = false;
      }
      vTaskDelay(pdMS_TO_TICKS(backoff_ms));
      if (backoff_ms < 30000) backoff_ms *= 2;
      esp_wifi_connect();
    }
    if (bits & STATUS_CHANGE_BIT) {
      publish_status(s_mqtt_client);
    }
  }
}
